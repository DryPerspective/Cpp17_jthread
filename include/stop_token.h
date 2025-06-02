#ifndef DP_STOP_SOURCE
#define DP_STOP_SOURCE

#include <atomic>
#include <memory>
#include <mutex>
#include <list>
#include <functional>
#include <optional>

#include "lock_free_shared_ptr.h"

namespace dp{

     template<typename Callback>
     class stop_callback;

    namespace detail {
        //NB: All operations on this class must either be
        //protected or atomic. Concurrent access per-instance may occur
        class stop_state {
            //Stop state is always lock free, because there may be high contention on multiple threads calling while(!stop_requested())
            //Querying stop state is wait-free, setting stop state may have some waiting if there are callbacks potentially being set.
            std::atomic<bool> m_stop_requested{ false };


            //Callbacks would be lock-free in an ideal world, however contention here should be lower than querying stop state
            //For now at least, we use a traditional lock to prevent a whole family of possible races and errors from occurring.
            class callback_state {
                std::size_t m_id;
                std::function<void()> m_callable;

            public:
                //In principle, the only users of this class should already have asserted that Func is callable with the right signature
                //So constraining it here is extra work for minimal gain
                template<typename Func>
                callback_state(std::size_t id, Func&& func) : m_id{ id }, m_callable{ std::forward<Func>(func) } {}

                inline void operator()() const {
                    std::invoke(m_callable);
                }

                inline std::size_t id() const {
                    return m_id;
                }
            };


            //Callback state variables
            std::size_t m_current_callback_id{ 0 };
            std::mutex m_mut{};
            std::list<callback_state> m_callbacks{};

            template<typename Callback>
            friend class dp::stop_callback;

            //A note to users - these functions are private for a reason and unprotected for a reason.
            //The way that we prevent races when registering and deregistering a callback is via double-checked locking.
            //As such, the callback functions which are using these functions must already hold the lock to protect the list.
            //So we can't also acquire the lock here otherwise it's deadlock
            template<typename Func>
            std::size_t register_callback(Func&& func) {
                auto this_id{ m_current_callback_id++ };
                m_callbacks.emplace_back(this_id, std::forward<Func>(func));
                return this_id;
            }

            void deregister_callback(std::size_t id);

            void execute_callbacks();           


        public:
            inline bool stop_requested() const noexcept {
                return m_stop_requested.load(std::memory_order_acquire);
            }
            inline void request_stop() noexcept {
                m_stop_requested.store(true, std::memory_order_release);

                auto lck{ std::lock_guard{m_mut} };
                execute_callbacks();
            }

        };

    }

    class stop_token{

        dp::lock_free_shared_ptr<detail::stop_state> m_state;

        friend class stop_source;
        template<typename Callback>
        friend class stop_callback;
        
        explicit stop_token(std::nullptr_t) noexcept : m_state{nullptr} {}

        public:
        stop_token() : m_state{std::make_shared<detail::stop_state>()} {}
        stop_token(const stop_token&) noexcept = default;
        stop_token(stop_token&&) noexcept = default;
        
        stop_token& operator=(const stop_token&) noexcept = default;
        stop_token& operator=(stop_token&&) noexcept = default;

        void swap(stop_token& other) noexcept;

        [[nodiscard]] bool stop_requested() const noexcept;

        [[nodiscard]] bool stop_possible() const noexcept;

        friend bool operator==(const stop_token& lhs, const stop_token& rhs) noexcept {
            return lhs.m_state.load() == rhs.m_state.load();
        }

        friend void swap(stop_token& lhs, stop_token& rhs) noexcept {
            lhs.swap(rhs);
        }


    };

    struct nostopstate_t{};
    constexpr inline nostopstate_t nostopstate{};

    class stop_source{
        dp::stop_token m_token;

        public:

        stop_source() : m_token{} {}
        explicit stop_source(nostopstate_t) noexcept : m_token{nullptr} {}
        stop_source(const stop_source&) noexcept = default;
        stop_source(stop_source&&) noexcept = default;

        stop_source& operator=(const stop_source&) noexcept = default;
        stop_source& operator=(stop_source&&) noexcept = default;

        bool request_stop() noexcept;

        void swap(stop_source& other) noexcept;

        stop_token get_token() const noexcept;

        [[nodiscard]] bool stop_requested() const noexcept;

        [[nodiscard]] bool stop_possible() const noexcept;

        friend bool operator==(const stop_source& lhs, const stop_source& rhs) noexcept{
            return lhs.m_token == rhs.m_token;
        }

        friend void swap(stop_source& lhs, stop_source& rhs) noexcept {
            lhs.swap(rhs);
        }



    };

    template<typename Callback>
    class stop_callback {

        static_assert(std::is_invocable_v<Callback>, "Callback given to stop_callback is not invocable with no parameters");
        static_assert(std::is_destructible_v<Callback>, "Callback given to stop_callback is not destructible");

        dp::stop_token m_token;
        std::optional<std::size_t> m_callback_id;
        

        //DRY from our near-duplicate constructors
        template<typename C>
        std::optional<std::size_t> register_or_invoke(C&& function) {
            static_assert(std::is_constructible_v<Callback, C>, "Callback is not constructible from provided argument types");
            //We check if the token holds a null ptr. Note that if it doesn't here it can't be changed to do so later so we don't need to manage that manually here
            if (!m_token.stop_possible()) {
                std::invoke(std::forward<C>(function));
                return std::nullopt;
            }
            //We do double-checked locking to ensure we don't race with another thread potentially executing all registered callbacks
            auto ptr{ m_token.m_state.load(std::memory_order_acquire) };
            if (!ptr->stop_requested()) {
                auto lck{ std::lock_guard{ptr->m_mut} };
                if (!ptr->stop_requested()) {
                    std::size_t this_id{ ptr->register_callback(std::forward<C>(function)) };
                    return this_id;
                }
                else {
                    std::invoke(std::forward<C>(function));
                    return std::nullopt;
                }
            }
            else {
                std::invoke(std::forward<C>(function));
                return std::nullopt;
            }
        }

    public:

        template<typename C>
        explicit stop_callback(const dp::stop_token& tok, C&& func) noexcept(std::is_nothrow_constructible_v<Callback, C>)
            : m_token{ tok }, m_callback_id{ register_or_invoke(std::forward<C>(func)) } {}

        template<typename C>
        explicit stop_callback(dp::stop_token&& tok, C&& func) noexcept(std::is_nothrow_constructible_v<Callback, C>)
            : m_token{ std::move(tok) }, m_callback_id{ register_or_invoke(std::forward<C>(func)) } {}

        stop_callback(const stop_callback&) = delete;
        stop_callback& operator=(const stop_callback&) = delete;
        stop_callback(stop_callback&&) = delete;
        stop_callback& operator=(stop_callback&&) = delete;

        ~stop_callback() noexcept {
            //If the callback is registered
            if (m_callback_id.has_value()) {
                //We know there's no way for ptr to be null, as were that the case m_callback_id would have no value
                //So we don't need to check that.
                auto ptr{ m_token.m_state.load(std::memory_order_acquire) };
                //We also do the same double-checked locking to ensure it is safely deregistered
                //or invoked by the calling thread
                if (!ptr->stop_requested()) {
                    auto lck{ std::lock_guard{ptr->m_mut} };
                    if (!ptr->stop_requested()) {
                        ptr->deregister_callback(*m_callback_id);
                    }
                }
            }
        }
    };

    template<typename Callback>
    stop_callback(stop_token, Callback) -> stop_callback<Callback>;



    


}



#endif