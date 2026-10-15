#ifndef ARCH_X86_HAL_ADDRESS_SPACE_HPP
#define ARCH_X86_HAL_ADDRESS_SPACE_HPP

#include <stdint.h>

namespace cock::core::hal {

using PhysicalRaw = uint32_t;
using VirtualRaw = uintptr_t;

constexpr uintptr_t USER_SPACE_LIMIT_ELF32 = 0xC0000000;
constexpr uintptr_t USER_SPACE_LIMIT_ELF64 = 0;
constexpr uintptr_t USER_SPACE_LIMIT =
	USER_SPACE_LIMIT_ELF32 > USER_SPACE_LIMIT_ELF64 ? USER_SPACE_LIMIT_ELF32
													: USER_SPACE_LIMIT_ELF64;
constexpr uintptr_t KERNEL_SPACE_BASE = 0xC0000000;
constexpr uintptr_t USER_MMAP_BASE = 0x60000000;

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_ADDRESS_SPACE_HPP
