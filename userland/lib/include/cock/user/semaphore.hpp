/**
 * @file semaphore.hpp
 * @brief Futex based counting semaphore.
 */
#ifndef COCK_USER_SEMAPHORE_HPP
#define COCK_USER_SEMAPHORE_HPP

#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

/**
 * @brief Counter that `wait` lowers and `post` raises; `wait` sleeps while it is zero.
 *
 * `post` makes a system call only when a thread sleeps in `wait`. Not fair: a thread that
 * calls `wait` just after a `post` can take the unit before a sleeper does. Only threads of
 * one process can share it. The object must stay alive and at a 4 byte aligned address while
 * threads use it.
 */
class Semaphore {
  public:
	/**
	 * @brief Creates a semaphore.
	 * @param initial Units available at the start.
	 */
	constexpr explicit Semaphore(uint32_t initial = NO_UNITS) : units(initial) {}

	/** @brief Takes one unit; sleeps while none is available. */
	void wait() {
		for (;;) {
			if (tryWait()) return;

			__atomic_add_fetch(&sleepers, 1, __ATOMIC_SEQ_CST);
			if (__atomic_load_n(&units, __ATOMIC_SEQ_CST) == NO_UNITS)
				futex_wait(&units, NO_UNITS);
			__atomic_sub_fetch(&sleepers, 1, __ATOMIC_SEQ_CST);
		}
	}

	/**
	 * @brief Takes one unit if one is available, without sleeping.
	 * @return true when a unit was taken.
	 */
	bool tryWait() {
		uint32_t available = __atomic_load_n(&units, __ATOMIC_SEQ_CST);
		while (available != NO_UNITS)
			if (__atomic_compare_exchange_n(&units, &available, available - ONE_UNIT, false,
											__ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
				return true;
		return false;
	}

	/** @brief Adds one unit and wakes one sleeping thread, if any. */
	void post() {
		__atomic_add_fetch(&units, ONE_UNIT, __ATOMIC_SEQ_CST);
		if (__atomic_load_n(&sleepers, __ATOMIC_SEQ_CST) != NO_SLEEPERS)
			futex_wake(&units, abi::process::FUTEX_WAKE_ONE);
	}

  private:
	/** Value of `units` when `wait` has to sleep. */
	static constexpr uint32_t NO_UNITS = 0;
	/** Value of `sleepers` when `post` needs no system call. */
	static constexpr uint32_t NO_SLEEPERS = 0;
	/** Units moved by one `wait` or `post`. */
	static constexpr uint32_t ONE_UNIT = 1;

	/** Futex word: units available. */
	uint32_t units;
	/** Threads inside `wait` that may be asleep. */
	uint32_t sleepers = NO_SLEEPERS;
};

} // namespace cock::user

#endif // !COCK_USER_SEMAPHORE_HPP
