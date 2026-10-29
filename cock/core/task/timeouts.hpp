/** @file
 * @brief Timer based deadlines for blocked threads: sleep, receive with
 * timeout, futex wait and synchronous calls.
 */
#ifndef CORE_TIMEOUTS_HPP
#define CORE_TIMEOUTS_HPP

#include <cock/core/task/thread.hpp>
#include <hal/cpu.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

/** @brief Outcome of Timeouts::wait. */
enum class TimeoutPhase {
	TIMED_OUT, /**< The deadline has passed, or was zero: do not block. */
	WAITING /**< The thread was put to sleep and its call will be restarted. */
};

/**
 * @brief Global list of armed deadlines, driven by the timer tick.
 *
 * Each thread holds its own link and wake tick, so arming never allocates. The
 * tick counter counts timer interrupts since boot.
 */
class Timeouts {
  public:
	/** @brief Milliseconds in one second, to convert to ticks. */
	static constexpr uint64_t MILLISECONDS_PER_SECOND = 1000;
	/** @brief Extra ticks added to every deadline so a wait is never shorter
	 * than requested. */
	static constexpr uint64_t TICK_MARGIN = 1;

	/** @return The current tick count. */
	static uint64_t now() { return ticks; }
	/**
	 * @brief Converts milliseconds to timer ticks, rounding up.
	 * @param milliseconds Duration.
	 * @return Number of ticks, at least covering the duration.
	 */
	static uint64_t ticksFor(uint32_t milliseconds);
	/** @return Number of threads with an armed deadline, mostly for tests. */
	static size_t armedCount();

	/**
	 * @brief Arms a deadline for a thread, replacing any previous one.
	 * @param thread Thread to arm, null is ignored.
	 * @param delay Delay in ticks, a margin tick is added.
	 */
	static void arm(Thread *thread, uint64_t delay);
	/**
	 * @brief Disarms a thread and resets its timeout state.
	 * @param thread Thread to disarm, null is ignored.
	 */
	static void cancel(Thread *thread);
	/**
	 * @brief Advances the clock by one tick and expires due deadlines.
	 *
	 * Expired threads are marked EXPIRED and made ready, or handed to the
	 * synchronous IPC code when they wait in a call.
	 * @note Called by the scheduler from the timer interrupt.
	 */
	static void onTick();
	/**
	 * @brief Blocks the current call until its deadline, restart style.
	 *
	 * The first time it arms the deadline, sets the blocked state and asks the
	 * scheduler to reschedule and restart the call. When the call runs again
	 * after the expiry it reports the timeout and clears the state.
	 *
	 * @param thread Calling thread.
	 * @param milliseconds Timeout. Zero never blocks.
	 * @param blockedState State to give the thread while it waits.
	 * @return TIMED_OUT when the deadline passed or the timeout is zero,
	 * WAITING when the thread was put to sleep.
	 * @note Blocks interrupts while it runs. Syscall context only.
	 */
	static TimeoutPhase wait(Thread &thread, uint32_t milliseconds,
							 ThreadState blockedState);

  private:
	static uint64_t ticks; /**< Timer ticks since boot. */
	static Thread *armed; /**< Head of the list of armed threads. */
};

} // namespace cock::core::task

#endif // !CORE_TIMEOUTS_HPP
