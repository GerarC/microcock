#ifndef COCK_USER_MUTEX_HPP
#define COCK_USER_MUTEX_HPP

#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

class Mutex {
  public:
	void lock() {
		uint32_t seen = UNLOCKED;
		if (__atomic_compare_exchange_n(&state, &seen, LOCKED, false, __ATOMIC_ACQUIRE,
										__ATOMIC_RELAXED))
			return;

		if (seen != CONTENDED) seen = __atomic_exchange_n(&state, CONTENDED, __ATOMIC_ACQUIRE);
		while (seen != UNLOCKED) {
			futex_wait(&state, CONTENDED);
			seen = __atomic_exchange_n(&state, CONTENDED, __ATOMIC_ACQUIRE);
		}
	}

	void unlock() {
		if (__atomic_exchange_n(&state, UNLOCKED, __ATOMIC_RELEASE) == CONTENDED)
			futex_wake(&state, WAKE_ONE);
	}

  private:
	static constexpr uint32_t UNLOCKED = 0;
	static constexpr uint32_t LOCKED = 1;
	static constexpr uint32_t CONTENDED = 2;
	static constexpr uint32_t WAKE_ONE = 1;

	uint32_t state = UNLOCKED;
};

} // namespace cock::user

#endif // !COCK_USER_MUTEX_HPP
