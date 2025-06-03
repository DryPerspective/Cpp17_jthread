#include "condition_variable.h"


namespace dp {


	void condition_variable_any::notify_one() noexcept {
		std::lock_guard lck{ *m_mut };
		m_cond.notify_one();
	}

	void condition_variable_any::notify_all() noexcept {
		std::lock_guard lck{ *m_mut };
		m_cond.notify_all();
	}



}