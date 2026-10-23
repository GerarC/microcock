#ifndef ARCH_X86_MEMORY_LAYOUT_HPP
#define ARCH_X86_MEMORY_LAYOUT_HPP

/**
 * @file
 * @brief Virtual memory map and two level paging geometry of i386 (4 KiB
 * pages, no PAE).
 *
 * Virtual layout: 0 to USER_TOP is user space; KERNEL_BASE and up is the shared
 * kernel half, with the first DIRECT_MAP_LIMIT bytes of RAM mapped at
 * KERNEL_BASE, the kernel heap in [KERNEL_HEAP_BASE, KERNEL_HEAP_LIMIT),
 * temporary mapping slots at TEMP_MAP_BASE, the boot window at
 * BOOT_WINDOW_BASE and the recursive page table mapping in the last 4 MiB.
 */

#include <hal/address_space.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Page size in bytes (4096). */
constexpr uint32_t PAGE_SIZE = core::hal::PAGE_SIZE;
/** @brief Mask of the offset inside a page. */
constexpr uint32_t PAGE_MASK = core::hal::PAGE_MASK;
/** @brief log2 of the page size. */
constexpr uint32_t PAGE_SHIFT = core::hal::PAGE_SHIFT;

static_assert(PAGE_SIZE == 0x1000, "x86 paging code assumes 4 KiB pages");
/** @brief Shift that turns a virtual address into a page directory index (each
 * directory entry covers 4 MiB). */
constexpr uint32_t PAGE_TABLE_SHIFT = 22;
/** @brief Entries in a page table or a page directory (1024). */
constexpr uint32_t ENTRIES_PER_TABLE = 0x400;
/** @brief Mask that extracts a 10 bit table or directory index. */
constexpr uint32_t ENTRY_INDEX_MASK = 0x3FF;

/** @brief Virtual address where the kernel half starts. */
constexpr uint32_t KERNEL_BASE =
	static_cast<uint32_t>(core::hal::KERNEL_SPACE_BASE);
/** @brief Exclusive upper bound of user virtual addresses. */
constexpr uint32_t USER_TOP =
	static_cast<uint32_t>(core::hal::USER_SPACE_LIMIT);

static_assert(USER_TOP == KERNEL_BASE,
			  "x86 user space must end where the kernel half begins");
/** @brief Physical bytes (4 MiB) mapped permanently at KERNEL_BASE by the boot
 * page directory. */
constexpr uint32_t DIRECT_MAP_LIMIT = 0x400000;
/** @brief First virtual address of the kernel heap. */
constexpr uint32_t KERNEL_HEAP_BASE = 0xD0000000;
/** @brief Exclusive end of the kernel heap. */
constexpr uint32_t KERNEL_HEAP_LIMIT = 0xE0000000;
/** @brief Virtual base of the temporary mapping slots used by the paging code
 * to edit foreign frames. */
constexpr uint32_t TEMP_MAP_BASE = 0xE0000000;
/** @brief Virtual base of the boot window (see BootMemory). */
constexpr uint32_t BOOT_WINDOW_BASE = 0xF0000000;

/** @brief Page directory slot that points back to the directory itself
 * (recursive mapping). */
constexpr uint32_t RECURSIVE_INDEX = 1023;
/** @brief Virtual address where every page table appears through the recursive
 * slot (4 MiB window). */
constexpr uint32_t RECURSIVE_TABLES = 0xFFC00000;
/** @brief Virtual address where the active page directory appears through the
 * recursive slot. */
constexpr uint32_t RECURSIVE_DIRECTORY = 0xFFFFF000;
/** @brief Page directory index of KERNEL_BASE: the first entry shared by every
 * address space. */
constexpr uint32_t FIRST_KERNEL_PDE = KERNEL_BASE >> PAGE_TABLE_SHIFT;

/** @brief Number of 4 KiB frames in 4 GiB; sizes the physical memory manager
 * bitmaps. */
constexpr uint32_t MAX_PHYSICAL_FRAMES = 0x100000;

} // namespace cock::arch::x86

#endif // !ARCH_X86_MEMORY_LAYOUT_HPP
