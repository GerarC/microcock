/**
 * @file condition_variable.hpp
 * @brief Futex based condition variable to use with `Mutex`.
 */
#ifndef COCK_USER_CONDITION_VARIABLE_HPP
#define COCK_USER_CONDITION_VARIABLE_HPP

#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <cock/user/mutex.hpp>
#include <stdint.h>

namespace cock::user {

/**
 * @brief Lets threads sleep until another thread changes a condition they check under a mutex.
 *
 * A wake counter is the futex word: `wait` reads it while it still holds the mutex, so a
 * `signal` or `broadcast` that arrives after the mutex is released and before the thread
 * sleeps changes the word and the sleep is skipped (no lost wakeup). A thread can return
 * from `wait` without a matching signal, so it must check its condition again in a loop:
 * `while (!ready) condition.wait(mutex);`. Only threads of one process can share it. The
 * object must stay alive and at a 4 byte aligned address while threads use it.
 */
class ConditionVariable {
  public:
	/**
	 * @brief Releases `mutex`, sleeps until a signal or broadcast, and takes `mutex` again.
	 * @tparam Lockable A type with `lock()` and `unlock()`, normally `Mutex`; a test can pass
	 *         one whose `unlock` signals, to place a signal exactly between the release and
	 *         the sleep.
	 * @param mutex A mutex held by the calling thread; it is held again when this returns.
	 */
	template <typename Lockable> void wait(Lockable &mutex) {
		uint32_t seen = __atomic_load_n(&wakeups, __ATOMIC_SEQ_CST);
		mutex.unlock();
		futex_wait(&wakeups, seen);
		mutex.lock();
	}

	/** @brief Wakes one thread that sleeps in `wait`; does nothing when none sleeps. */
	void signal() {
		__atomic_add_fetch(&wakeups, 1, __ATOMIC_SEQ_CST);
		futex_wake(&wakeups, abi::process::FUTEX_WAKE_ONE);
	}

	/** @brief Wakes every thread that sleeps in `wait`. */
	void broadcast() {
		__atomic_add_fetch(&wakeups, 1, __ATOMIC_SEQ_CST);
		futex_wake(&wakeups, abi::process::FUTEX_WAKE_ALL);
	}

  private:
	/** Futex word: counts signals and broadcasts; wraps around by design. */
	uint32_t wakeups = 0;
};

} // namespace cock::user

#endif // !COCK_USER_CONDITION_VARIABLE_HPP
