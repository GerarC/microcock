#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/mm/placement.hpp>
#include <cock/core/task/process.hpp>
#include <hal/address_space.hpp>
#include <hal/page.hpp>

namespace cock::core::syscall {

using memory::VirtAddr;
using memory::VirtualMemoryManager;
using task::Process;
using task::Thread;

namespace {

constexpr size_t MAX_PLACEMENT_ATTEMPTS = 64;

} // namespace

uintptr_t user_image_limit(const Thread &thread) {
	return thread.getUserSpaceLimit().raw() - task::USER_STACK_SIZE;
}

VirtAddr find_user_placement(Thread &current, size_t pages) {
	Process &process = *current.getProcess();
	VirtAddr search_from(hal::USER_MMAP_BASE);
	VirtAddr search_limit(user_image_limit(current));

	for (size_t attempt = 0; attempt < MAX_PLACEMENT_ATTEMPTS; attempt++) {
		VirtAddr base = process.findFreeRange(pages, search_from, search_limit);
		if (base.isNull()) return VirtAddr();
		if (VirtualMemoryManager::isRangeUnmapped(base, pages)) return base;

		search_from = base + hal::PAGE_SIZE;
	}
	return VirtAddr();
}

} // namespace cock::core::syscall
