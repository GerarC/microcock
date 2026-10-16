#ifndef ARCH_X86_HAL_ADDRESS_SPACE_HPP
#define ARCH_X86_HAL_ADDRESS_SPACE_HPP

/**
 * @file
 * @brief HAL: raw address types and the user/kernel split of the virtual
 * address space.
 *
 * Port requirement: every architecture must provide this header with the same
 * names. It must define PhysicalRaw and VirtualRaw wide enough for the
 * architecture, the highest user address bound per executable class
 * (USER_SPACE_LIMIT_ELF32 and USER_SPACE_LIMIT_ELF64, zero when that class is
 * unsupported), USER_SPACE_LIMIT, KERNEL_SPACE_BASE and USER_MMAP_BASE. The
 * core uses them to validate every user pointer and to place mappings.
 */

#include <stdint.h>

namespace cock::core::hal {

/** @brief Integer type that holds a physical address (32 bit on i386 without
 * PAE). */
using PhysicalRaw = uint32_t;
/** @brief Integer type that holds a virtual address (pointer sized). */
using VirtualRaw = uintptr_t;

/** @brief First address a 32 bit user program may not touch (3 GiB split). */
constexpr uintptr_t USER_SPACE_LIMIT_ELF32 = 0xC0000000;
/** @brief User address bound for 64 bit programs; zero because i386 cannot run
 * them. */
constexpr uintptr_t USER_SPACE_LIMIT_ELF64 = 0;
/** @brief Exclusive upper bound of user addresses: the larger of the ELF32 and
 * ELF64 bounds. */
constexpr uintptr_t USER_SPACE_LIMIT =
	USER_SPACE_LIMIT_ELF32 > USER_SPACE_LIMIT_ELF64 ? USER_SPACE_LIMIT_ELF32
													: USER_SPACE_LIMIT_ELF64;
/** @brief Virtual address where the kernel half starts (higher half kernel). */
constexpr uintptr_t KERNEL_SPACE_BASE = 0xC0000000;
/** @brief Default base address the kernel searches from when it places user
 * mappings (mmap, shared memory). */
constexpr uintptr_t USER_MMAP_BASE = 0x60000000;

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_ADDRESS_SPACE_HPP
