#ifndef ARCH_X86_MEMORY_HPP
#define ARCH_X86_MEMORY_HPP
#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/memory/layout.hpp>
#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

class PhysicalMemoryManager {
  public:
	static void init(const MBInfo *boot_info,
					 core::memory::PhysAddr boot_info_phys);

	static core::memory::PhysAddr allocFrame();
	static void freeFrame(core::memory::PhysAddr frame);
	static void releaseBootFrame(core::memory::PhysAddr frame);
	static bool isRam(core::memory::PhysAddr frame);
	static size_t freeFrameCount();
};

} // namespace cock::arch::x86

#endif // !MEMORY_HPP
