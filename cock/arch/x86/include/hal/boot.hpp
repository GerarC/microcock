#ifndef HAL_BOOT_HPP
#define HAL_BOOT_HPP

/**
 * @file
 * @brief HAL: hooks into the architecture specific boot path.
 *
 * Port requirement: provide boot_memory_release() in an architecture source
 * file. If the port needs no temporary early mapping it may be an empty
 * function.
 */

namespace cock::core::hal {

/**
 * @brief Drops the temporary mappings used to read bootloader data.
 *
 * Called once by the core after the boot information, modules and memory map
 * have been consumed. Pointers obtained from the boot mapping facility must not
 * be used afterwards. On x86 this forwards to arch::x86::BootMemory::release(),
 * which clears the boot window page table and flushes its TLB entries with
 * `invlpg`. Runs in Ring 0 during boot.
 */
void boot_memory_release();

} // namespace cock::core::hal

#endif // !HAL_BOOT_HPP
