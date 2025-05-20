#include "stop_token.h"

namespace dp {

//---STOP TOKEN-------------------------------------------------

void stop_token::swap(stop_token& other) noexcept {
    m_state.swap(other.m_state);
}

bool stop_token::stop_requested() const noexcept {
    auto ptr = m_state.load(std::memory_order_acquire);
    return ptr && ptr->stop_requested();
}


bool stop_token::stop_possible() const noexcept {
    auto ptr = m_state.load(std::memory_order_acquire);
    return ptr != nullptr;
}

//---STOP SOURCE-----------------------------------------------

bool stop_source::request_stop() noexcept {
    auto ptr = m_token.m_state.load(std::memory_order_acquire);
    if (ptr) {
        ptr->request_stop();
        return true;
    }
    return false;
}

void stop_source::swap(stop_source& other) noexcept {
    m_token.swap(other.m_token);
}

stop_token stop_source::get_token() const noexcept {
    return m_token;
}

bool stop_source::stop_requested() const noexcept {
    return m_token.stop_requested();
}

bool stop_source::stop_possible() const noexcept {
    return m_token.stop_possible();
}




}