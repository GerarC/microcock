/**
 * @file sched_metrics.hpp
 * @brief Layout of the figures that `SYS_SCHED_METRICS` writes, shared by the kernel and user space.
 */
#ifndef SHARED_COCK_ABI_SCHED_METRICS_HPP
#define SHARED_COCK_ABI_SCHED_METRICS_HPP

#include <stdint.h>

namespace cock::abi::sched {

/**
 * Scheduler figures of the calling thread, all in timer ticks.
 */
struct Metrics {
	/** Ticks from the moment the thread became ready for the first time until it first ran. */
	uint32_t responseTicks;
	/** Ticks the thread was running when the timer fired (time on the CPU, rounded to ticks). */
	uint32_t cpuTicks;
	/** Ticks from the creation of the thread until the call. */
	uint32_t turnaroundTicks;
};

} // namespace cock::abi::sched

#endif // !SHARED_COCK_ABI_SCHED_METRICS_HPP
