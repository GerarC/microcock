/** @file
 * @brief Per thread scheduler figures, present only in a build with
 * `SCHED_METRICS=1`.
 */
#ifndef CORE_THREAD_METRICS_HPP
#define CORE_THREAD_METRICS_HPP

#include <stdint.h>

namespace cock::core::task {

#ifdef COCK_SCHED_METRICS

/** @brief True when the kernel was built with the scheduler metrics. */
constexpr bool SCHED_METRICS_ENABLED = true;

/** @brief Figures kept for one thread, in timer ticks. */
struct ThreadMetrics {
	uint64_t createdTick = 0; /**< Tick at which the thread was created. */
	uint64_t readyTick = 0; /**< Tick at which the thread last became ready. */
	uint64_t responseTicks = 0; /**< Ready to first run; valid once `responded`. */
	uint64_t cpuTicks = 0; /**< Ticks that fired while the thread was running. */
	bool responded = false; /**< True after the first dispatch. */
};

#else

/** @brief True when the kernel was built with the scheduler metrics. */
constexpr bool SCHED_METRICS_ENABLED = false;

/** @brief Empty without the metrics, so a thread does not grow. */
struct ThreadMetrics {};

#endif

} // namespace cock::core::task

#endif // !CORE_THREAD_METRICS_HPP
