#include <cock/arch/x86/boot/boot_memory.hpp>
#include <cock/arch/x86/memory/layout.hpp>
#include <hal/cpu.hpp>

namespace cock::arch::x86 {

using core::memory::PhysAddr;

PageTable BootMemory::window;
uint32_t BootMemory::nextPage = 0;

void BootMemory::init() {
	nextPage = 0;
	uint32_t window_phys = reinterpret_cast<uint32_t>(&window) - KERNEL_BASE;
	page_directory[BOOT_WINDOW_BASE >> PAGE_TABLE_SHIFT] =
		window_phys | PRESENT | WRITEABLE;
}

void *BootMemory::map(PhysAddr address, uint32_t size) {
	uint32_t phys = address.raw();
	if (size == 0) size = 1;

	uint32_t end = phys + size;
	if (end < phys) return nullptr;
	if (end <= DIRECT_MAP_LIMIT)
		return reinterpret_cast<void *>(phys + KERNEL_BASE);

	uint32_t first = phys & ~PAGE_MASK;
	uint32_t pages = ((end - first) + PAGE_MASK) >> PAGE_SHIFT;
	if (pages > ENTRIES_PER_TABLE || nextPage + pages > ENTRIES_PER_TABLE)
		return nullptr;

	uint32_t base = BOOT_WINDOW_BASE + nextPage * PAGE_SIZE;
	for (uint32_t idx = 0; idx < pages; idx++) {
		window.entries[nextPage + idx] =
			(first + idx * PAGE_SIZE) | PRESENT | WRITEABLE;
		core::hal::invalidate_page(base + idx * PAGE_SIZE);
	}
	nextPage += pages;

	return reinterpret_cast<void *>(base + (phys & PAGE_MASK));
}

} // namespace cock::arch::x86
