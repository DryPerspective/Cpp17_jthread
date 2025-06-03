#ifndef DP_CONDITION_VARIABLE
#define DP_CONDITION_VARIABLE

/*
*	An implementation for condition_variable_any which is able to be notified by a dp::stop_token
* 
*	Full documentation found at: https://github.com/DryPerspective/Cpp17_jthread/wiki/condition_variable
*
*/
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <utility>

#include "stop_token.h"


namespace dp {


	//For the sake of consistency when swapping between regular condition_variables and *_any we make condition_variable findable here
	using std::condition_variable;
	using std::cv_status;
	using std::notify_all_at_thread_exit;



	//Condition variable any can be a somewhat expensive class to manage with many implementations holding their own locks
	//Equally, we need to make sure that our own lock is acquired on attempts to notify as well as attempts to wait
	//As such, it is probably simpler to reimplement condition_variable_any from scratch then to have to ensure that we don't get a mismatch
	//of one set of waits acquiring an internal lock and another acquiring an external lock.
	class condition_variable_any {

		std::condition_variable     m_cond;
		std::shared_ptr<std::mutex> m_mut;


		//We need a lot of well-planned locking and unlocking of mutexes as we go. This helper class will serve the same purpose.
		//There's a consistent pattern here - while looping or waiting, locks must be unlocked and relocked every time.
		//This class allows us to schedule that relative to other locks to keep things consistent.
		template<typename Lock>
		class scoped_unlock {
			Lock& m_lck;

		public:
			explicit scoped_unlock(Lock& lck) : m_lck{ lck } { lck.unlock(); }
			~scoped_unlock() { m_lck.lock(); }

			scoped_unlock(const scoped_unlock&) = delete;
			scoped_unlock& operator=(const scoped_unlock&) = delete;
			scoped_unlock(scoped_unlock&&) = delete;
			scoped_unlock& operator=(scoped_unlock&&) = delete;
		};
		template<typename Lock>
		scoped_unlock(Lock) -> scoped_unlock<Lock>;



	public:

		condition_variable_any() : m_cond{}, m_mut{ std::make_shared<std::mutex>() } {}

		condition_variable_any(const condition_variable_any&) = delete;
		condition_variable_any& operator=(const condition_variable_any&) = delete;
		condition_variable_any(condition_variable_any&&) = delete;
		condition_variable_any& operator=(condition_variable_any&&) = delete;

		void notify_one() noexcept;
		void notify_all() noexcept;

		template<typename Lock>
		void wait(Lock& lock) {
			auto mut{ m_mut };
			std::unique_lock outer_lock{ *mut };
			scoped_unlock param_unlock{ lock };
			//We now need to ensure that the outer_lock is unlocked before the param_lock is locked
			std::unique_lock inner_lock{ std::move(outer_lock) };
			m_cond.wait(inner_lock);
		}

		template<typename Lock, typename Pred>
		void wait(Lock& lock, Pred pred) {
			while (!pred()) {
				wait(lock);
			}
		}

		template<typename Lock, typename Pred>
		bool wait(Lock& lock, dp::stop_token token, Pred pred) {

			//Initial check for if a stop was already requested before this function was called
			if (token.stop_requested()) {
				return pred();
			}

			//Then we need to do a delicate balancing act to ensure we don't miss our wait.
			//We need to ensure that either:
			// * We are waiting when notify is called
			// * The notify_all() call itself cannot proceed until we are waiting
			//This we manage with a similar pattern to callbacks - we hold the lock which the call to notify
			//will want to acquire. If we see a requested stop we know we can exit the function. If we don't,
			//we know the lock will be held until wait() and then notify_all() will be able to grab it
			//Either way we don't miss our wait.

			//Silence unnecessary warnings. We never use the callback object but it's vital that it's there
			[[maybe_unused]] dp::stop_callback callback{ token, [this] {notify_all(); } };

			std::shared_ptr mut{ m_mut };
			while (!pred()) {
				std::unique_lock outer_lock{ *mut };
				if (token.stop_requested()){
					return false;
				}
				scoped_unlock param_unlock{ lock };
				std::unique_lock inner_lock{ std::move(outer_lock) };
				m_cond.wait(inner_lock);
			}
			return true;
		}

		template<typename Lock, typename Clock, typename Duration>
		std::cv_status wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& end_time){
			std::shared_ptr mut{ m_mut };
			std::unique_lock outer_lock{ *mut };
			scoped_unlock param_unlock{ lock };
			std::unique_lock inner_lock{ std::move(outer_lock) };
			m_cond.wait_until(inner_lock, end_time);
		}

		template<typename Lock, typename Clock, typename Duration, typename Pred>
		bool wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& end_time, Pred predicate) {
			while (!predicate()) {
				if (wait_until(lock, end_time) == std::cv_status::timeout) {
					return predicate();
				}
				return true;
			}
		}

		template<typename Lock, typename Clock, typename Duration, typename Pred>
		bool wait_until(Lock& lock, dp::stop_token token, const std::chrono::time_point<Clock, Duration>& end_time, Pred predicate) {
			//As with wait(), we check if a stop was requested before we even got to this function
			if (token.stop_requested()) {
				return predicate();
			}

			//Otherwise we do our dance to make sure we're either waiting or blocking the notify call until we are
			[[maybe_unused]] dp::stop_callback callback{ token, [this] {notify_all(); } };
			std::shared_ptr mut{ m_mut };
			while (!predicate()) {
				bool stop{ false };
				{
					std::unique_lock outer_lock{ *mut };
					if (token.stop_requested()) {
						return false;
					}
					scoped_unlock param_unlock{ lock };
					std::unique_lock inner_lock{ std::move(outer_lock) };
					const std::cv_status status{ m_cond.wait_until(inner_lock, end_time) };
					stop = (status == std::cv_status::timeout || token.stop_requested());
				}
				if (stop) {
					return predicate();
				}
			}
			return true;
		}

		template<typename Lock, typename Rep, typename Period>
		std::cv_status wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& wait_time) {
			return wait_until(lock, std::chrono::steady_clock::now() + wait_time);
		}

		template<typename Lock, typename Rep, typename Period, typename Pred>
		bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& wait_time, Pred pred) {
			return wait_until(lock, std::chrono::steady_clock::now() + wait_time, std::move(pred));
		}

		template<typename Lock, typename Rep, typename Period, typename Pred>
		bool wait_for(Lock& lock, dp::stop_token token, const std::chrono::duration<Rep, Period>& wait_time, Pred pred) {
			return wait_until(lock, std::move(token), std::chrono::steady_clock::now() + wait_time, std::move(pred));
		}


	};



}




#endif