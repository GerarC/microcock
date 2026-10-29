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
 * @brief Global variable holding the id of the process that owns the screen.
 * * Used by sys_write to route standard output from the kernel to that process
 * as `OUTPUT_WRITE_STR` messages. The kernel does not know which program it
 * is: it is the first process that maps the screen memory (see `sys_mmap`).
 * Zero means no owner: output goes straight to the kernel console. The
 * value is cleared when the owner dies. This debug channel goes away with M2.
 */
extern uint32_t debug_output_pid;

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
