#ifndef DP_STOP_SOURCE
#define DP_STOP_SOURCE

#include <atomic>
#include <memory>

#include "lock_free_shared_ptr.h"

namespace dp{


    //NB: All operations on this class must either be
    //protected or atomic. Concurrent access per-instance may occur
    class stop_state{ 
        std::atomic<bool> m_stop_requested{false};
    public:
        inline bool stop_requested() const noexcept{
            return m_stop_requested.load(std::memory_order_acquire);
        }
        inline void request_stop() noexcept{
            m_stop_requested.store(true, std::memory_order_release);
        }

    };

    class stop_token{

        dp::lock_free_shared_ptr<stop_state> m_state;

        friend class stop_source;
        
        explicit stop_token(std::nullptr_t) noexcept : m_state{nullptr} {}

        public:
        stop_token() : m_state{std::make_shared<stop_state>()} {}
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



    };

    void swap(stop_source& lhs, stop_source& rhs) noexcept;
    void swap(stop_token& lhs, stop_token& rhs) noexcept;

    


}


//This is formal UB starting in C++20 but you really should be using std::stop_source in that case
namespace std{
    template<>
    inline void swap(dp::stop_token& lhs, dp::stop_token& rhs) noexcept {
        lhs.swap(rhs);
    }
    template<>
    inline void swap(dp::stop_source& lhs, dp::stop_source& rhs) noexcept {
        lhs.swap(rhs);
    }
}

#endif