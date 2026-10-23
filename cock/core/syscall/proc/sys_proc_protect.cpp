#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <hal/page.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using memory::VirtAddr;
using memory::VirtualMemoryManager;
using memory::VMMPermission;
using task::PendingProcesses;

uintptr_t sys_proc_protect(uintptr_t handle, uintptr_t address, uintptr_t pages, uintptr_t,
						   uintptr_t, uintptr_t) {
	PendingProcesses::Entry *entry = pending_of_caller(handle);
	if (entry == nullptr || !image_pages_fit(*entry->process, address, pages))
		return PROC_FAILURE;

	VirtAddr limit = entry->process->getUserSpaceLimit();
	PendingSpaceScope scope(*entry->process);
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(address), pages << hal::PAGE_SHIFT,
												false, limit))
		return PROC_FAILURE;

	for (size_t page = 0; page < pages; page++) {
		VirtAddr start(address + (page << hal::PAGE_SHIFT));
		if (!VirtualMemoryManager::protectPage(start, VMMPermission::USER_CODE))
			return PROC_FAILURE;
	}
	return PROC_SUCCESS;
}

} // namespace cock::core::syscall
