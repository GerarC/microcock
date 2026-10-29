#ifndef ARCH_X86_HAL_EXECUTABLE_HPP
#define ARCH_X86_HAL_EXECUTABLE_HPP

/**
 * @file
 * @brief HAL: which ELF executables this architecture can run.
 *
 * Port requirement: define every constant below. The ELF loader in the core
 * compares the header fields e_machine and EI_DATA against these values and
 * rejects classes whose SUPPORTED flag is false.
 */

#include <stdint.h>

namespace cock::core::hal {

/** @brief True when 32 bit ELF (ELFCLASS32) programs can run. */
constexpr bool ELF32_SUPPORTED = true;
/** @brief True when 64 bit ELF (ELFCLASS64) programs can run; false on i386. */
constexpr bool ELF64_SUPPORTED = false;
/** @brief Expected e_machine of a 32 bit program (3 is EM_386). */
constexpr uint16_t ELF32_MACHINE = 3;
/** @brief Expected e_machine of a 64 bit program (62 is EM_X86_64). */
constexpr uint16_t ELF64_MACHINE = 62;
/** @brief Expected EI_DATA byte (1 is ELFDATA2LSB, little endian). */
constexpr uint8_t ELF_DATA_ENCODING = 1;

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_EXECUTABLE_HPP
