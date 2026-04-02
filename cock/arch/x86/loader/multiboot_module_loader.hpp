#ifndef ARCH_X86_MULTIBOOT_MODULE_LOADER_HPP
#define ARCH_X86_MULTIBOOT_MODULE_LOADER_HPP

#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/core/boot/module.hpp>
#include <stddef.h>

namespace cock::arch::x86 {

using core::boot::BootModule;

class MultibootModuleLoader {
  public:
	static void init(MBInfo *boot_info);
	static const BootModule *getModules() { return modules; }
	static size_t getCount() {return modCount;}

  private:
	static constexpr size_t MAX_MODULE_NUM = 0x10;
	static BootModule modules[MAX_MODULE_NUM];
	static size_t modCount;
};
} // namespace cock::arch::x86

#endif // !ARCH_X86_MULTIBOOT_MODULE_LOADER_HPP
