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

uintptr_t sys_proc_map(uintptr_t handle, uintptr_t address, uintptr_t pages, uintptr_t,
					   uintptr_t, uintptr_t) {
	PendingProcesses::Entry *entry = pending_of_caller(handle);
	if (entry == nullptr || !image_pages_fit(*entry->process, address, pages))
		return PROC_FAILURE;

	size_t total = abi::process::MAX_PENDING_IMAGE_BYTES >> hal::PAGE_SHIFT;
	if (pages > total - entry->pages) return PROC_FAILURE;
	entry->pages += pages;

	PendingSpaceScope scope(*entry->process);
	if (!VirtualMemoryManager::isRangeUnmapped(VirtAddr(address), pages)) return PROC_FAILURE;
	VirtAddr mapped = VirtualMemoryManager::mapPagesAt(
		VirtAddr(address), pages, VMMPermission::USER_DATA, entry->process->getUserSpaceLimit());
	return mapped.isNull() ? PROC_FAILURE : PROC_SUCCESS;
}

} // namespace cock::core::syscall
