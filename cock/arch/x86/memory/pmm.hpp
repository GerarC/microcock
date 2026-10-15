#ifndef ARCH_X86_MEMORY_HPP
#define ARCH_X86_MEMORY_HPP
#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

static constexpr uint32_t NULL_PTR = 0x0;

class PhysicalMemoryManager {
  public:
	static void init(const MBInfo *boot_info, uint32_t boot_info_phys);
	static void invalidatePage(uint32_t virtual_address);

	static uint32_t allocFrame();
	static void freeFrame(uint32_t phys_addr);

	static bool mapPage(uint32_t phys, uint32_t virt, uint32_t flags);
	static void unmapPage(uint32_t virt);
	static uint32_t getPageFlags(uint32_t virt);
	static bool updatePageFlags(uint32_t virt, uint32_t flags);
	static uint32_t getPhysicalAddress(uint32_t virt);
	static bool isRangeAccessible(uint32_t virt, uint32_t length, bool user,
								  bool write);

	static uint32_t createAddressSpace();
	static void destroyAddressSpace(uint32_t pd_phys);
	static uint32_t getKernelDirectory();
};

} // namespace cock::arch::x86

#endif // !MEMORY_HPP
