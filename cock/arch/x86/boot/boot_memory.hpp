#ifndef ARCH_X86_BOOT_MEMORY_HPP
#define ARCH_X86_BOOT_MEMORY_HPP

/**
 * @file
 * @brief Early boot access to physical memory that the boot page directory does
 * not map.
 *
 * Before the physical memory manager exists the kernel can only dereference the
 * first DIRECT_MAP_LIMIT bytes (mapped at KERNEL_BASE by the boot page
 * directory). Multiboot structures and modules may live above that limit, so
 * this class exposes a small temporary window of one page table at
 * BOOT_WINDOW_BASE. The window is bump allocated and is closed for good by
 * release() once the boot data has been consumed.
 */

#include <cock/arch/x86/memory/paging.hpp>
#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/**
 * @brief Temporary kernel mapping window used while parsing boot information.
 *
 * Runs in Ring 0 during early boot, single CPU, interrupts not yet relevant.
 * Not thread safe.
 */
class BootMemory {
  public:
	/**
	 * @brief Installs the window page table in the boot page directory.
	 *
	 * Writes the page directory entry for BOOT_WINDOW_BASE (present and
	 * writable) pointing to the static window table and rewinds the bump
	 * allocator. Must run once, before the first call to map().
	 */
	static void init();

	/**
	 * @brief Makes a physical range readable and writable by the kernel.
	 *
	 * Ranges that end at or below DIRECT_MAP_LIMIT are returned through the
	 * permanent KERNEL_BASE direct map without consuming window slots. Any
	 * other range is mapped page by page in the window (present and writable)
	 * and the TLB entry of every new page is invalidated with `invlpg`.
	 * Slots are never reused until release().
	 *
	 * @param phys Physical address of the first byte to reach.
	 * @param size Number of bytes needed; zero is treated as one byte.
	 * @return Kernel virtual pointer to the byte at `phys`, or nullptr when the
	 * range wraps around 4 GiB, is larger than one page table (4 MiB) or the
	 * window has no free slots left (or has already been released).
	 */
	static void *map(core::memory::PhysAddr phys, uint32_t size);

	/**
	 * @brief Unmaps every window page and closes the window.
	 *
	 * Clears the used page table entries, invalidates them with `invlpg` and
	 * marks the window as full so later map() calls above DIRECT_MAP_LIMIT
	 * fail. Every pointer previously returned for an address above the direct
	 * map becomes invalid.
	 */
	static void release();

  private:
	/** @brief Page table that backs the boot window (4 MiB of virtual space). */
	static PageTable window;
	/** @brief Index of the next free slot of #window (bump allocator). */
	static uint32_t nextPage;
};

} // namespace cock::arch::x86

#endif // !ARCH_X86_BOOT_MEMORY_HPP
