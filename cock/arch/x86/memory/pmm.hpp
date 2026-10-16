#ifndef ARCH_X86_MEMORY_HPP
#define ARCH_X86_MEMORY_HPP

/**
 * @file
 * @brief Physical memory manager: a bitmap frame allocator for 4 KiB frames.
 *
 * Built from the Multiboot memory map. Frames below 1 MiB, the kernel image and
 * the bootloader structures (info, memory map, modules, command lines) start
 * out reserved. Allocation and release take an interrupt guard, so they are
 * safe against interrupt handlers on a single CPU.
 */

#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/memory/layout.hpp>
#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Static facade over the frame bitmaps. */
class PhysicalMemoryManager {
  public:
	/**
	 * @brief Builds the frame bitmaps from the bootloader information.
	 *
	 * Uses the Multiboot memory map when present, otherwise `mem_upper`, and
	 * panics when neither is available. Marks as used the first MiB, the kernel
	 * image and the boot structures. Must run once, after BootMemory::init(),
	 * since it reads boot data through BootMemory::map().
	 *
	 * @param boot_info Kernel virtual pointer to the Multiboot information.
	 * @param boot_info_phys Physical address of that same structure, so its
	 * frames can be reserved.
	 */
	static void init(const MBInfo *boot_info,
					 core::memory::PhysAddr boot_info_phys);

	/**
	 * @brief Takes one free frame.
	 *
	 * The contents of the frame are undefined; clear it before exposing it to
	 * user mode.
	 *
	 * @return Physical address of the frame, or a null PhysAddr when memory is
	 * exhausted.
	 */
	static core::memory::PhysAddr allocFrame();

	/**
	 * @brief Returns a frame obtained from allocFrame().
	 *
	 * Frames that were not handed out by allocFrame() (for example kernel or
	 * boot frames) and out of range addresses are ignored, so a double free is
	 * harmless.
	 *
	 * @param frame Physical address of the frame.
	 */
	static void freeFrame(core::memory::PhysAddr frame);

	/**
	 * @brief Recycles a frame that was reserved at boot (for example a
	 * bootloader module that has been consumed).
	 *
	 * Only RAM frames that are currently reserved and were not allocated by
	 * allocFrame() are released; anything else is ignored.
	 *
	 * @param frame Physical address of the frame.
	 */
	static void releaseBootFrame(core::memory::PhysAddr frame);

	/**
	 * @brief Tells whether an address is backed by RAM according to the boot
	 * memory map.
	 * @param frame Physical address to test.
	 * @return True for RAM, false for holes, devices and addresses beyond
	 * MAX_PHYSICAL_FRAMES.
	 */
	static bool isRam(core::memory::PhysAddr frame);

	/**
	 * @brief Reports how many frames are currently free.
	 * @return Free frame count.
	 */
	static size_t freeFrameCount();
};

} // namespace cock::arch::x86

#endif // !MEMORY_HPP
