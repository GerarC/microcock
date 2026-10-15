#include <cock/abi/process.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::Scheduler;
using core::task::Thread;
using core::task::ThreadPriority;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr size_t ENTRY_PROBE_BYTES = 1;

} // namespace

uintptr_t sys_thread_create(uintptr_t entry, uintptr_t stack_base,
							uintptr_t stack_size, uintptr_t, uintptr_t,
							uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || current->getProcess()->isKernel()) return FAILURE;
	if (stack_size < abi::process::MIN_THREAD_STACK_BYTES) return FAILURE;

	VirtAddr limit = current->getUserSpaceLimit();
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(entry), ENTRY_PROBE_BYTES,
												false, limit) ||
		!VirtualMemoryManager::isUserRangeValid(VirtAddr(stack_base), stack_size,
												true, limit))
		return FAILURE;

	Thread *thread =
		Thread::createUser(*current->getProcess(), VirtAddr(entry),
						   VirtAddr(stack_base), stack_size, ThreadPriority::NORMAL);
	if (thread == nullptr) return FAILURE;

	Scheduler::addThread(thread);
	return thread->getId();
}

uintptr_t sys_gettid(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
					 uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	return current ? current->getId() : FAILURE;
}

} // namespace cock::core::syscall
