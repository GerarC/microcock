#include <cock/abi/process.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/futex.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::Futex;
using core::task::Scheduler;
using core::task::Thread;
using core::task::ThreadState;
using core::task::Timeouts;
using core::task::TimeoutState;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr size_t FUTEX_WORD_BYTES = sizeof(uint32_t);

bool futexWordOk(const Thread &thread, uintptr_t address) {
	return address != task::NO_FUTEX_ADDRESS &&
		   address % abi::process::FUTEX_WORD_ALIGNMENT == 0 &&
		   VirtualMemoryManager::isUserRangeValid(VirtAddr(address), FUTEX_WORD_BYTES,
												  false, thread.getUserSpaceLimit());
}

} // namespace

uintptr_t sys_futex_wait(uintptr_t address, uintptr_t expected, uintptr_t milliseconds,
						 uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;

	if (current->takeFutexWake()) {
		Timeouts::cancel(current);
		return abi::process::FUTEX_WOKEN;
	}
	if (current->isFutexWaiting() &&
		current->getTimeoutState() == TimeoutState::EXPIRED) {
		Futex::remove(current);
		Timeouts::cancel(current);
		return abi::process::FUTEX_TIMED_OUT;
	}

	bool forever = milliseconds == abi::process::FUTEX_NO_TIMEOUT;
	if (!futexWordOk(*current, address)) return FAILURE;
	if (!forever && milliseconds > MAX_TIMEOUT_MILLISECONDS) return FAILURE;

	if (!current->isFutexWaiting()) {
		uint32_t value = *reinterpret_cast<const volatile uint32_t *>(address);
		if (value != static_cast<uint32_t>(expected))
			return abi::process::FUTEX_VALUE_CHANGED;
		if (!forever && milliseconds == 0) return abi::process::FUTEX_TIMED_OUT;
		Futex::enqueue(current, address);
	}

	if (forever) {
		current->setState(ThreadState::BLOCKED);
		Scheduler::requestReschedule();
		Scheduler::requestRestart();
	} else {
		Timeouts::wait(*current, static_cast<uint32_t>(milliseconds),
					   ThreadState::BLOCKED);
	}
	return abi::process::FUTEX_WOKEN;
}

uintptr_t sys_futex_wake(uintptr_t address, uintptr_t count, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !futexWordOk(*current, address)) return FAILURE;
	return Futex::wake(current->getProcess(), address, count);
}

} // namespace cock::core::syscall
