#ifndef ARCH_X86_SEGMENTS_HPP
#define ARCH_X86_SEGMENTS_HPP

#include <stdint.h>

namespace cock::arch::x86 {

constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
constexpr uint16_t KERNEL_DATA_SELECTOR = 0x10;
constexpr uint16_t USER_CODE_SELECTOR = 0x1B;
constexpr uint16_t USER_DATA_SELECTOR = 0x23;
constexpr uint16_t TSS_SELECTOR = 0x2B;
constexpr uint16_t DOUBLE_FAULT_TSS_SELECTOR = 0x33;
constexpr uint16_t NMI_TSS_SELECTOR = 0x3B;

constexpr uint32_t EFLAGS_INTERRUPTS_ENABLED = 0x202;
constexpr uint32_t EFLAGS_RESERVED_BIT = 0x2;

} // namespace cock::arch::x86

#endif // !ARCH_X86_SEGMENTS_HPP
