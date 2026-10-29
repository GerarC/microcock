#ifndef ARCH_X86_SEGMENTS_HPP
#define ARCH_X86_SEGMENTS_HPP

/**
 * @file
 * @brief Segment selector values of the flat GDT and the EFLAGS bits used when
 * building initial contexts.
 *
 * A selector is `index << 3 | TI << 2 | RPL`. The GDT slot order is: null,
 * kernel code, kernel data, user code, user data, TSS, double fault TSS, NMI
 * TSS (see GDT::init()).
 */

#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Ring 0 code segment (GDT index 1, RPL 0). */
constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
/** @brief Ring 0 data and stack segment (GDT index 2, RPL 0). */
constexpr uint16_t KERNEL_DATA_SELECTOR = 0x10;
/** @brief Ring 3 code segment (GDT index 3, RPL 3). */
constexpr uint16_t USER_CODE_SELECTOR = 0x1B;
/** @brief Ring 3 data and stack segment (GDT index 4, RPL 3). */
constexpr uint16_t USER_DATA_SELECTOR = 0x23;
/** @brief Task register selector of the running kernel TSS (GDT index 5). */
constexpr uint16_t TSS_SELECTOR = 0x2B;
/** @brief Task gate target used by vector 8, #DF (GDT index 6). */
constexpr uint16_t DOUBLE_FAULT_TSS_SELECTOR = 0x33;
/** @brief Task gate target used by vector 2, NMI (GDT index 7). */
constexpr uint16_t NMI_TSS_SELECTOR = 0x3B;

/** @brief EFLAGS for a new context: IF set (bit 9) plus the always one bit 1. */
constexpr uint32_t EFLAGS_INTERRUPTS_ENABLED = 0x202;
/** @brief EFLAGS bit 1, reserved and always read as one. */
constexpr uint32_t EFLAGS_RESERVED_BIT = 0x2;

} // namespace cock::arch::x86

#endif // !ARCH_X86_SEGMENTS_HPP
