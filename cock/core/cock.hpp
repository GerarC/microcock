#ifndef COCK_HPP
#define COCK_HPP

#include <cock/core/boot/module.hpp>
#include <stddef.h>

namespace cock {

/** @brief Macro for an infinite loop, commonly used for kernel halts and panics. */
#define FOR_ETERNAL for (;;)

using core::boot::BootModule;

/**
 * @brief Global variable holding the Process ID of the Userland VGA Server.
 * * Used by sys_write to route standard output from the kernel to the userland display driver.
 */
extern uint32_t vga_server_pid;

extern "C" {
/**
 * @brief Architecture-independent main entry point for the kernel.
 * * Called by the architecture-specific initialization code (e.g., core_init) after 
 * * basic hardware (GDT, IDT, Paging) is set up.
 * @param modules Array of boot modules (e.g., ELF binaries) loaded by the bootloader.
 * @param mod_count Number of modules in the array.
 */
void cock_main(const BootModule *modules, size_t mod_count);
}

} // namespace cock

#endif // !COCK_HPP
