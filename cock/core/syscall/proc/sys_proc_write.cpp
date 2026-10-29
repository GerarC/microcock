#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <hal/page.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using memory::VirtAddr;
using memory::VirtualMemoryManager;
using task::PendingProcesses;
using task::Scheduler;

namespace {

constexpr size_t BOUNCE_PAGES = 1;
constexpr size_t BOUNCE_BYTES = BOUNCE_PAGES << hal::PAGE_SHIFT;

}

uintptr_t sys_proc_write(uintptr_t handle, uintptr_t address, uintptr_t source,
						 uintptr_t length, uintptr_t, uintptr_t) {
	PendingProcesses::Entry *entry = pending_of_caller(handle);
	if (entry == nullptr || length > abi::process::MAX_PENDING_IMAGE_BYTES ||
		!image_range_fits(*entry->process, address, length))
		return PROC_FAILURE;

	task::Thread *current = Scheduler::getCurrentThread();
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(source), length, false,
												current->getUserSpaceLimit()))
		return PROC_FAILURE;

	void *bounce = VirtualMemoryManager::allocPages(BOUNCE_PAGES);
	if (bounce == nullptr) return PROC_FAILURE;

	VirtAddr limit = entry->process->getUserSpaceLimit();
	uintptr_t result = PROC_SUCCESS;
	size_t done = 0;
	while (done < length && result == PROC_SUCCESS) {
		size_t chunk = length - done;
		if (chunk > BOUNCE_BYTES) chunk = BOUNCE_BYTES;
		memcpy(bounce, reinterpret_cast<const void *>(source + done), chunk);

		PendingSpaceScope scope(*entry->process);
		if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(address + done), chunk, true, limit))
			result = PROC_FAILURE;
		else
			memcpy(reinterpret_cast<void *>(address + done), bounce, chunk);
		done += chunk;
	}
	VirtualMemoryManager::freePages(bounce, BOUNCE_PAGES);
	return result;
}

} // namespace cock::core::syscall
