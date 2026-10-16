/** @file
 * @brief Kernel entry point called by the architecture code, and the global
 * state it shares with the syscall layer.
 */
#ifndef COCK_HPP
#define COCK_HPP

#include <cock/core/boot/module.hpp>
#include <cock/utils/macros.hpp>
#include <stddef.h>

namespace cock {

using core::boot::BootModule;

/**
 * @brief Global variable holding the Process ID of the Userland VGA Server.
 * * Used by sys_write to route standard output from the kernel to the userland display driver.
 * Zero means no server: output goes straight to the kernel console. The
 * value is cleared when the server dies.
 */
extern uint32_t vga_server_pid;

extern "C" {
/**
 * @brief Architecture-independent main entry point for the kernel.
 * * Called by the architecture-specific initialization code (e.g., core_init) after
 * * basic hardware (GDT, IDT, Paging) is set up.
 * @param modules Array of boot modules (e.g., ELF binaries) loaded by the bootloader.
 * @param mod_count Number of modules in the array.
 * @note Starts the kernel services and the first user processes. It does not
 * return in normal operation.
 */
void cock_main(const BootModule *modules, size_t mod_count);
}

} // namespace cock

#endif // !COCK_HPP
