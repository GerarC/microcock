/**
 * @file mutex.hpp
 * @brief Futex based mutex for threads of one process.
 */
#ifndef COCK_USER_MUTEX_HPP
#define COCK_USER_MUTEX_HPP

#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

/**
 * @brief Mutex with three states (unlocked, locked, contended) built on `SYS_FUTEX_WAIT`
 *        and `SYS_FUTEX_WAKE`.
 *
 * The uncontended path is one atomic operation and no system call. Only threads of the
 * same process can share it. Not recursive and not fair. The object must stay alive and
 * at a 4 byte aligned address while threads use it.
 */
class Mutex {
  public:
	/**
	 * @brief Takes the mutex. Blocks (sleeps in the kernel) while another thread holds it.
	 *
	 * Locking twice from one thread deadlocks.
	 */
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

	/**
	 * @brief Releases the mutex and wakes one waiter when there is contention.
	 *
	 * Must be called by the thread that holds the lock; this is not checked.
	 */
	void unlock() {
		if (__atomic_exchange_n(&state, UNLOCKED, __ATOMIC_RELEASE) == CONTENDED)
			futex_wake(&state, WAKE_ONE);
	}

  private:
	/** State: nobody holds the mutex. */
	static constexpr uint32_t UNLOCKED = 0;
	/** State: held, no waiter known. */
	static constexpr uint32_t LOCKED = 1;
	/** State: held and at least one thread may be sleeping on it. */
	static constexpr uint32_t CONTENDED = 2;
	/** Number of waiters woken by `unlock`. */
	static constexpr uint32_t WAKE_ONE = 1;

	/** Futex word that holds the state. */
	uint32_t state = UNLOCKED;
};

} // namespace cock::user

#endif // !COCK_USER_MUTEX_HPP
