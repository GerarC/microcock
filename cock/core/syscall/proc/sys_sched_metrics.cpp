#ifdef COCK_SCHED_METRICS

#include <cock/abi/sched_metrics.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/sched_metrics.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::SchedMetrics;
using core::task::Scheduler;
using core::task::Thread;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

} // namespace

uintptr_t sys_sched_metrics(uintptr_t buffer, uintptr_t, uintptr_t, uintptr_t,
							uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	if (!VirtualMemoryManager::isUserRangeValid(
			VirtAddr(buffer), sizeof(abi::sched::Metrics), true,
			current->getUserSpaceLimit()))
		return FAILURE;

	abi::sched::Metrics metrics = {};
	if (!SchedMetrics::read(*current, metrics)) return FAILURE;
	memcpy(reinterpret_cast<void *>(buffer), &metrics, sizeof(metrics));
	return SUCCESS;
}

} // namespace cock::core::syscall

#endif
