#ifndef ARCH_X86_MULTIBOOT_MODULE_LOADER_HPP
#define ARCH_X86_MULTIBOOT_MODULE_LOADER_HPP

/**
 * @file
 * @brief Converts the Multiboot module list into the architecture neutral
 * BootModule array consumed by the core.
 */

#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/core/boot/module.hpp>
#include <stddef.h>

namespace cock::arch::x86 {

using core::boot::BootModule;

/**
 * @brief Handles the extraction of loaded modules from the Multiboot
 * information structure.
 * * GRUB loads modules (like userland drivers/apps) into physical memory and
 * passes their
 * * boundaries to the kernel. This class parses and stores that information
 * safely.
 *
 * Ring 0, early boot, single threaded. The resulting pointers use the boot
 * window and stay valid only until BootMemory::release().
 */
class MultibootModuleLoader {
  public:
	/**
	 * @brief Parses the Multiboot Info structure to locate and store module
	 * details.
	 *
	 * Resets the module count, then, when the bootloader reported modules,
	 * maps the module list, each module image and its name string through
	 * BootMemory::map(). Modules that cannot be mapped are logged and skipped;
	 * parsing stops at MAX_MODULE_NUM. Modules without a command line get an
	 * empty name.
	 *
	 * @param boot_info Pointer to the Multiboot information structure provided
	 * by GRUB. A null pointer or one without the modules flag leaves the count
	 * at zero.
	 */
	static void init(const MBInfo *boot_info);

	/**
	 * @brief Retrieves the array of parsed boot modules.
	 * @return Pointer to the internal BootModule array.
	 */
	static const BootModule *getModules() { return modules; }

	/**
	 * @brief Retrieves the total number of modules loaded by the bootloader.
	 * @return The module count.
	 */
	static size_t getCount() { return modCount; }

  private:
	/** @brief Maximum number of boot modules the kernel can track. */
	static constexpr size_t MAX_MODULE_NUM = 0x10;

	/** @brief Parsed modules, valid for the first #modCount entries. */
	static BootModule modules[MAX_MODULE_NUM];
	/** @brief Number of valid entries in #modules. */
	static size_t modCount;
};

} // namespace cock::arch::x86

#endif // !ARCH_X86_MULTIBOOT_MODULE_LOADER_HPP
