/**
 * @file executable.hpp
 * @brief What a program image must look like for this architecture, as the user loader needs it.
 *
 * The kernel checks these values against its own (`hal/executable.hpp` and `hal/address_space.hpp`)
 * with `static_assert`, so the loader in `userland/lib` and the kernel never disagree.
 */
#ifndef SHARED_COCK_ABI_EXECUTABLE_HPP
#define SHARED_COCK_ABI_EXECUTABLE_HPP

#include <stdint.h>

namespace cock::abi::executable {

/** `e_machine` of the programs the user loader accepts (3 is `EM_386`). */
constexpr uint16_t ELF_MACHINE = 3;
/** `EI_DATA` of the programs the user loader accepts (1 is little endian). */
constexpr uint8_t ELF_DATA_ENCODING = 1;
/** `EI_CLASS` of the programs the user loader accepts (1 is 32 bit). */
constexpr uint8_t ELF_CLASS = 1;
/** First address a 32 bit program may not use; the user stack ends there. */
constexpr uint32_t USER_SPACE_LIMIT = 0xC0000000;

} // namespace cock::abi::executable

#endif // !SHARED_COCK_ABI_EXECUTABLE_HPP
