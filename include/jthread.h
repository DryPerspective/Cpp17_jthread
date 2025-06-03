#ifndef DP_JTHREAD
#define DP_JTHREAD

//Our lock free jthread class. The implementation wraps a thread and a stop token and defers functionality to the underlying implementation
//Note that we follow the standard behaviour and do not impose synchronisation on the jthread object itself. 
//Your code could cause it to race if not protected

//Full documentation is found at: https://github.com/DryPerspective/Cpp17_jthread/wiki/jthread

#include <thread>
#include <type_traits>
#include "stop_token.h"

namespace dp {


	class jthread {

		std::thread m_thread;
		dp::stop_source m_stop;


	public:
		using id = std::thread::id;

		jthread() noexcept : m_thread{}, m_stop{ dp::nostopstate } {}
		
		//Not copyable
		jthread(const jthread&) = delete;
		jthread& operator=(const jthread&) = delete;

		//But noexcept moveable
		jthread(jthread&&) noexcept = default;
		jthread& operator=(jthread&&) noexcept = default;

		template<typename Func, typename... Args>
		jthread(Func&& func, Args&&... args) : m_thread{}, m_stop{} {
			//First, ill-formed checks
			static_assert(std::is_constructible_v<std::decay_t<Func>, Func>, "Invalid function type used to construct jthread");
			static_assert((std::is_constructible_v<std::decay_t<Args>, Args> && ...), "Invalid argument types used to construct jthread");
			static_assert(
				std::is_invocable_v<std::decay_t<Func>, std::decay_t<Args>...> ||
				std::is_invocable_v<std::decay_t<Func>, dp::stop_token, std::decay_t<Args>...>,
				"Function passed to jthread is not invocable with provided arguments"
				);

			//And now to selectively apply the stop_token
			if constexpr (std::is_invocable_v<std::decay_t<Func>, dp::stop_token, std::decay_t<Args>...>) {
				m_thread = std::thread{ std::forward<Func>(func), m_stop.get_token(), std::forward<Args>(args)... };
			}
			else {
				m_thread = std::thread{ std::forward<Func>(func), std::forward<Args>(args)... };
			}
		}

		~jthread() {
			if (joinable()) {
				m_stop.request_stop();
				m_thread.join();
			}
		}

		bool joinable() const noexcept;
		jthread::id get_id() const noexcept;

		static unsigned int hardware_concurrency() noexcept;

		void join();
		void detach();
		void swap(jthread& other) noexcept;

		dp::stop_source get_stop_source() noexcept;
		dp::stop_token get_stop_token() const noexcept;
		bool request_stop() noexcept;


		friend void swap(jthread& lhs, jthread& rhs) noexcept {
			lhs.swap(rhs);
		}




	};





}



#endif