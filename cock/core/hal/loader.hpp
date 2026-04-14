#ifndef CORE_HAL_LOADER_HPP
#define CORE_HAL_LOADER_HPP

#include <stdint.h>

namespace cock::core::hal {

/**
 * @brief Parses and loads an Executable and Linkable Format (ELF) binary into
 * memory.
 * * Maps the required segments (Code, Data, BSS) into the target virtual
 * address space.
 * @param file_in_mem Pointer to the raw ELF file loaded in memory by the
 * bootloader.
 * @param address_space The physical address of the Page Directory where the ELF
 * should be mapped.
 * @return The virtual entry point address of the loaded executable.
 */
uintptr_t load_executable(void *file_in_mem, uintptr_t address_space);

} // namespace cock::core::hal

#endif
