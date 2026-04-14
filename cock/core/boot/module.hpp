#ifndef CORE_BOOT_MODULE_HPP
#define CORE_BOOT_MODULE_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::boot {
/**
 * @brief Represents a loaded module provided by the Multiboot compliant
 * bootloader (e.g., GRUB).
 * * Usually represents loaded ELF binaries (Userland programs/drivers) sitting
 * in memory,
 * * waiting to be mapped to a Page Directory and scheduled.
 */
struct BootModule {
	void *start_address; /**< Virtual address where the module is loaded in
							memory. */
	size_t size;		 /**< Size of the module in bytes. */
	const char *name; /**< Name or command line string passed to the module. */
};
} // namespace cock::core::boot

#endif
