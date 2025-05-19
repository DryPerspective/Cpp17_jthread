#ifndef DP_LOCK_FREE_SHARED_PTR
#define DP_LOCK_FREE_SHARED_PTR

#include <memory>

//A basic lock free shared pointer
//Access to the pointer is always free of data races, but access to the pointee is not.
//Note that this is a LOCK FREE shared_ptr, not an ATOMIC shared ptr. Not all operations are atomic.
//Consequently, we *never* allow access to the internal pointer directly, we always
//make a copy via an atomic load.
//If in C++20 and up abandon this header and use std::atomic<std::shared_ptr<T>> instead

namespace dp{
    template<typename T>
    class lock_free_shared_ptr{

        std::shared_ptr<T> m_ptr;

        public:
        
        lock_free_shared_ptr() noexcept : m_ptr{nullptr} {}
        lock_free_shared_ptr(std::nullptr_t) noexcept : m_ptr{nullptr} {}
        explicit lock_free_shared_ptr(std::shared_ptr<T> ptr) : m_ptr{std::move(ptr)} {}

        //seq_cst is heavier but more safe
        lock_free_shared_ptr(const lock_free_shared_ptr& other) : m_ptr{std::atomic_load(&other.m_ptr)} {}
        lock_free_shared_ptr& operator=(const lock_free_shared_ptr& other){
            auto copy{other};
            this->swap(copy);
            return *this;
        }

        lock_free_shared_ptr(lock_free_shared_ptr&& other) noexcept {
            //We can't just store the result of std::move(other.m_ptr) as it may result in a data race
            auto other_ptr = std::atomic_load(&other.m_ptr);
            store(std::move(other_ptr));
            other.store(std::shared_ptr<T>{nullptr});
        }
        lock_free_shared_ptr& operator=(lock_free_shared_ptr&& other) noexcept{
            auto other_ptr = std::atomic_load(&other.m_ptr);
            store(std::move(other_ptr));
            other.store(std::shared_ptr<T>{nullptr});
            return *this;
        }

        ~lock_free_shared_ptr() = default;

        void swap(lock_free_shared_ptr& other) noexcept {
            auto this_old = std::atomic_load_explicit(&m_ptr, std::memory_order_acquire);
            auto other_old = std::atomic_exchange_explicit(&other.m_ptr, this_old, std::memory_order_acq_rel);

            //Try to store other_old into this->m_ptr only if it's still this_old
            while (!std::atomic_compare_exchange_weak_explicit(
                &m_ptr, &this_old, other_old,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                //Failed: m_ptr was changed by another thread. Update this_old and retry.
                std::atomic_store_explicit(&other.m_ptr, this_old, std::memory_order_release);
                this_old = std::atomic_load_explicit(&m_ptr, std::memory_order_acquire);
                other_old = std::atomic_exchange_explicit(&other.m_ptr, this_old, std::memory_order_acq_rel);
            }
        }

        [[nodiscard]] std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept{
            return std::atomic_load_explicit(&m_ptr, order);
        }
        void store(std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept{
            std::atomic_store_explicit(&m_ptr, std::move(desired), order);
        }

        [[nodiscard]] std::shared_ptr<T> exchange(std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept{
            return std::atomic_exchange_explicit(&m_ptr, std::move(desired), order);
        }

        [[nodiscard]] bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order success, std::memory_order failure) noexcept{
            return std::atomic_compare_exchange_weak_explicit(&m_ptr, expected, std::move(desired), success, failure);
        }

        bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order success, std::memory_order failure) noexcept{
            return std::atomic_compare_exchange_strong_explicit(&m_ptr, expected, std::move(desired), success, failure);
        }

        [[nodiscard]] bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
            auto fail_order = (order == std::memory_order_acq_rel) ? std::memory_order_acquire
                                : (order == std::memory_order_release) ? std::memory_order_relaxed : order;
            return compare_exchange_weak(expected, std::move(desired), order, fail_order);
        }

        bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
            auto fail_order = (order == std::memory_order_acq_rel) ? std::memory_order_acquire
                                : (order == std::memory_order_release) ? std::memory_order_relaxed : order;
            return compare_exchange_strong(expected, std::move(desired), order, fail_order);
        }
    };
}





#endif