#include "jthread.h"

namespace dp {

	bool jthread::joinable() const noexcept {
		return m_thread.joinable();
	}

	jthread::id jthread::get_id() const noexcept {
		return m_thread.get_id();
	}

	unsigned int jthread::hardware_concurrency() noexcept {
		return std::thread::hardware_concurrency();
	}

	void jthread::join() {
		m_thread.join();
	}

	void jthread::detach() {
		m_thread.detach();
		m_stop = dp::stop_source{ dp::nostopstate };
	}

	void jthread::swap(dp::jthread& other) noexcept {
		m_thread.swap(other.m_thread);
		m_stop.swap(other.m_stop);
	}

	dp::stop_source jthread::get_stop_source() noexcept {
		return m_stop;
	}

	dp::stop_token jthread::get_stop_token() const noexcept {
		return m_stop.get_token();
	}

	bool jthread::request_stop() noexcept {
		return m_stop.request_stop();
	}



}