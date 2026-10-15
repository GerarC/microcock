#ifndef ARCH_X86_BOOT_MEMORY_HPP
#define ARCH_X86_BOOT_MEMORY_HPP

#include <cock/arch/x86/memory/paging.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

class BootMemory {
  public:
	static void init();
	static void *map(uint32_t phys, uint32_t size);

  private:
	static PageTable window;
	static uint32_t nextPage;
};

} // namespace cock::arch::x86

#endif // !ARCH_X86_BOOT_MEMORY_HPP
