#ifndef ARCH_X86_MEMORY_LAYOUT_HPP
#define ARCH_X86_MEMORY_LAYOUT_HPP

#include <hal/address_space.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

constexpr uint32_t PAGE_SIZE = core::hal::PAGE_SIZE;
constexpr uint32_t PAGE_MASK = core::hal::PAGE_MASK;
constexpr uint32_t PAGE_SHIFT = core::hal::PAGE_SHIFT;

static_assert(PAGE_SIZE == 0x1000, "x86 paging code assumes 4 KiB pages");
constexpr uint32_t PAGE_TABLE_SHIFT = 22;
constexpr uint32_t ENTRIES_PER_TABLE = 0x400;
constexpr uint32_t ENTRY_INDEX_MASK = 0x3FF;

constexpr uint32_t KERNEL_BASE =
	static_cast<uint32_t>(core::hal::KERNEL_SPACE_BASE);
constexpr uint32_t USER_TOP =
	static_cast<uint32_t>(core::hal::USER_SPACE_LIMIT);

static_assert(USER_TOP == KERNEL_BASE,
			  "x86 user space must end where the kernel half begins");
constexpr uint32_t DIRECT_MAP_LIMIT = 0x400000;
constexpr uint32_t KERNEL_HEAP_BASE = 0xD0000000;
constexpr uint32_t KERNEL_HEAP_LIMIT = 0xE0000000;
constexpr uint32_t TEMP_MAP_BASE = 0xE0000000;
constexpr uint32_t BOOT_WINDOW_BASE = 0xF0000000;

constexpr uint32_t RECURSIVE_INDEX = 1023;
constexpr uint32_t RECURSIVE_TABLES = 0xFFC00000;
constexpr uint32_t RECURSIVE_DIRECTORY = 0xFFFFF000;
constexpr uint32_t FIRST_KERNEL_PDE = KERNEL_BASE >> PAGE_TABLE_SHIFT;

constexpr uint32_t MAX_PHYSICAL_FRAMES = 0x100000;

} // namespace cock::arch::x86

#endif // !ARCH_X86_MEMORY_LAYOUT_HPP
