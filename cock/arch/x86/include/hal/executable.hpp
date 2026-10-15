#ifndef ARCH_X86_HAL_EXECUTABLE_HPP
#define ARCH_X86_HAL_EXECUTABLE_HPP

#include <stdint.h>

namespace cock::core::hal {

constexpr bool ELF32_SUPPORTED = true;
constexpr bool ELF64_SUPPORTED = false;
constexpr uint16_t ELF32_MACHINE = 3;
constexpr uint16_t ELF64_MACHINE = 62;
constexpr uint8_t ELF_DATA_ENCODING = 1;

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_EXECUTABLE_HPP
