#ifndef ARCH_X86_HELPERS_HPP
#define ARCH_X86_HELPERS_HPP

#include <stdint.h>

namespace cock::arch::x86::utils {

/**
 * @brief Structure used to capture the CPU context (registers) during an interrupt.
 * * Pushed to the stack by the ISR/IRQ assembly stubs before calling C/C++ handlers.
 */
typedef struct _interrupt_registers_t {
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} InterruptRegisters;

/**
 * @brief Dumps the captured CPU registers to the standard output for debugging.
 * @param r Pointer to the interrupt registers structure.
 */
void print_interrupt_registers(const InterruptRegisters *r);

}

#endif // !HELPERS_HPP
