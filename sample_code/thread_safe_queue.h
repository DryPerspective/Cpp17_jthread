#ifndef DP_JTHREAD_THREAD_SAFE_QUEUE
#define DP_JTHREAD_THREAD_SAFE_QUEUE

/* 
*	A sample usage of the dp::jthread family of code - a thread safe queue
*  
*   A dp::stop_token is passed to the queue on construction, and any waiting functions in the queue
*   will be notified and return if a stop is requested on the source.
*   As this is a sample, we don't go too far into complex structure like lock free ring buffers. We provide a
*   simple thread_safe queue which will not deadlock on a wait when a stop has been requested.
*/

#include <deque>
#include <queue>
#include <mutex>
#include <utility>
#include <type_traits>

#include "stop_token.h"
#include "condition_variable.h"

namespace dp::thread_safe {

	template<typename T, typename Container = std::deque<T>>
	class queue {

		std::queue<T, Container> m_queue{};
		mutable std::mutex m_mut{};
		
		dp::condition_variable_any m_cond{};

	public:

		using queue_type =		decltype(m_queue);
		using container_type =	typename Container;
		using value_type =		typename Container::value_type;
		using size_type =		typename Container::size_type;
		using reference =		typename Container::reference;
		using const_reference = typename Container::const_reference;

		template<typename T>
		using out_parameter = T&;

		explicit queue() = default;
		explicit queue(queue_type in_queue) : m_queue{ std::move(in_queue) } {}

		queue(const queue& other) {
			std::lock_guard lck{ other.m_mut };
			m_queue = other.m_queue;
		}

		queue& operator=(const queue& other) {
			if (this == &other) return;
			queue temp{ other };
			this->swap(temp);
			return *this;
		}

		//NB: can't be noexcept movable, as locking may throw
		queue(queue&& other) {
			std::lock_guard lck{ other.m_mut };
			m_queue = std::move(other.m_queue);
		}
		queue& operator=(queue&& other) {
			std::scoped_lock lck{ m_mut, other.m_mut };
			m_queue = std::move(other.m_queue);;
			return *this;
		}


		void swap(queue& other) noexcept {
			if (this == &other) return;
			std::scoped_lock lck{ m_mut, other.m_mut };
			swap(m_queue, other.m_queue);			
		}

		bool empty() const {
			std::lock_guard lck{ m_mut };
			return m_queue.empty();
		}

		void push(value_type in_val) {
			std::lock_guard lck{ m_mut };
			m_queue.push(std::move(in_val));
			m_cond.notify_one();
		}
		template<typename... Args>
		void emplace(Args&&... args) {
			static_assert(std::is_constructible_v<value_type, Args...>, "Values passed to emplace cannot be used to construct the queue's value type");
			std::lock_guard lck{ m_mut };
			m_queue.emplace(std::forward<Args>(args)...);
			m_cond.notify_one();
		}

		//try_pop will pop the next element in the queue if there is one and return true
		//if there is no one it will return false without waiting
		[[nodiscard]] bool try_pop(T& out_parameter) {
			std::lock_guard lck{ m_mut };
			if (m_queue.empty()) return false;

			out_parameter = std::move(m_queue.front());
			m_queue.pop();
			return true;
		}



		//wait_pop will wait until there is an element in the queue to pop.
		//The thread will sleep if there is no value and be woken when a call to push() notifies it
		void wait_pop(T& out_parameter) {
			std::unique_lock lck{ m_mut };
			m_cond.wait(lck, [this] {return !m_queue.empty(); });

			out_parameter = std::move(m_queue.front());
			m_queue.pop();
		}
		
		//Overload to wake when a stop is requested on the stop token
		void wait_pop(dp::stop_token token, T& out_parameter) {
			std::unique_lock lck{ m_mut };
			m_cond.wait(lck, token, [this] {return !m_queue.empty(); });
			if (token.stop_requested()) return;

			out_parameter = std::move(m_queue.front());
			m_queue.pop();
		}


		friend void swap(queue& lhs, queue& rhs) noexcept {
			lhs.swap(rhs);
		}

	};

	template<typename T, typename Container>
	queue(std::queue<T, Container>) -> queue<T, Container>;















}

#endif