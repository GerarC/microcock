/** @file
 * @brief Hooks that record response time, CPU time and turnaround time of
 * threads (OSTEP chapter 7).
 *
 * Every hook is a template whose body is compiled only when `SCHED_METRICS_ENABLED`
 * is true, so a kernel built without `SCHED_METRICS=1` has no extra code in the
 * scheduler and no extra bytes in a thread.
 */
#ifndef CORE_SCHED_METRICS_HPP
#define CORE_SCHED_METRICS_HPP

#include <cock/abi/sched_metrics.hpp>
#include <cock/core/task/thread_metrics.hpp>
#include <cock/core/task/timeouts.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::task {

/**
 * @brief Static hooks called by the thread, the scheduler and the task manager.
 *
 * Interrupt context safe: no allocation, no blocking.
 */
class SchedMetrics {
  public:
	/**
	 * @brief Starts the clocks of a new thread.
	 * @param thread The thread, which is ready.
	 */
	template <typename ThreadType>
	static void onCreate(ThreadType &thread) {
		if constexpr (SCHED_METRICS_ENABLED) {
			uint64_t now = Timeouts::now();
			thread.metrics.createdTick = now;
			thread.metrics.readyTick = now;
		}
	}

	/**
	 * @brief Notes the tick at which a thread entered a ready queue.
	 * @param thread The thread.
	 */
	template <typename ThreadType>
	static void onReady(ThreadType &thread) {
		if constexpr (SCHED_METRICS_ENABLED)
			thread.metrics.readyTick = Timeouts::now();
	}

	/**
	 * @brief Closes the response time at the first dispatch of a thread.
	 * @param thread The thread that is about to run.
	 */
	template <typename ThreadType>
	static void onDispatch(ThreadType &thread) {
		if constexpr (SCHED_METRICS_ENABLED) {
			if (thread.metrics.responded) return;
			thread.metrics.responseTicks = Timeouts::now() - thread.metrics.readyTick;
			thread.metrics.responded = true;
		}
	}

	/**
	 * @brief Charges one tick to the running thread.
	 * @param current Thread running when the timer fired, or null.
	 * @param idle The idle thread, which is not charged.
	 */
	template <typename ThreadType>
	static void onTick(ThreadType *current, ThreadType *idle) {
		if constexpr (SCHED_METRICS_ENABLED) {
			if (current != nullptr && current != idle &&
				current->getState() == ThreadState::RUNNING)
				current->metrics.cpuTicks++;
		}
	}

	/**
	 * @brief Logs the figures of a thread that ended, at trace level.
	 * @param thread The dead thread.
	 */
	template <typename ThreadType>
	static void onExit(ThreadType &thread) {
		if constexpr (SCHED_METRICS_ENABLED) {
			utils::Logger::trace(
				"Thread %d metrics: response %d cpu %d turnaround %d ticks",
				thread.getId(), static_cast<int>(thread.metrics.responseTicks),
				static_cast<int>(thread.metrics.cpuTicks),
				static_cast<int>(Timeouts::now() - thread.metrics.createdTick));
		}
	}

	/**
	 * @brief Copies the figures of a thread to the shared layout.
	 * @param thread The thread to read, normally the caller.
	 * @param out Receives the figures.
	 * @return False when the kernel was built without the metrics.
	 */
	template <typename ThreadType>
	static bool read(const ThreadType &thread, abi::sched::Metrics &out) {
		if constexpr (SCHED_METRICS_ENABLED) {
			out.responseTicks = static_cast<uint32_t>(thread.metrics.responseTicks);
			out.cpuTicks = static_cast<uint32_t>(thread.metrics.cpuTicks);
			out.turnaroundTicks =
				static_cast<uint32_t>(Timeouts::now() - thread.metrics.createdTick);
			return true;
		} else {
			(void)thread;
			(void)out;
			return false;
		}
	}
};

} // namespace cock::core::task

#endif // !CORE_SCHED_METRICS_HPP
