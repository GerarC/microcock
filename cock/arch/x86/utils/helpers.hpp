#ifndef ARCH_X86_HELPERS_HPP
#define ARCH_X86_HELPERS_HPP

/**
 * @file
 * @brief Layout of the register frame pushed by the interrupt stubs and a
 * debug dump helper.
 */

#include <stdint.h>

namespace cock::arch::x86::utils {

/**
 * @brief Structure used to capture the CPU context (registers) during an interrupt.
 * * Pushed to the stack by the ISR/IRQ assembly stubs before calling C/C++ handlers.
 *
 * Field order matches the push order of the stubs (lowest address first): CR2
 * and DS saved by software, the `pusha` block, the vector number and error code
 * pushed by the stub or the CPU, then the frame pushed by the CPU itself. The
 * `useresp` and `ss` fields exist only when the interrupt came from Ring 3.
 * Changing this struct requires changing the assembly stubs.
 */
typedef struct _interrupt_registers_t {
    /** CR2 at entry (faulting address on a page fault). */
    uint32_t cr2;
    /** Data segment selector at entry. */
    uint32_t ds;
    /** General purpose registers as saved by `pusha`; esp is the value before the push. */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    /** Interrupt vector number and CPU error code (zero for vectors without one). */
    uint32_t int_no, err_code;
    /** Resume address, code selector (low two bits give the interrupted ring), EFLAGS, and user stack pointer and segment (the last two valid only for Ring 3 origin). */
    uint32_t eip, cs, eflags, useresp, ss;
} InterruptRegisters;

/**
 * @brief Dumps the captured CPU registers to the standard output for debugging.
 *
 * Prints with printf, so the output ends up on the kernel console.
 *
 * @param r Pointer to the interrupt registers structure.
 */
void print_interrupt_registers(const InterruptRegisters *r);

}

#endif // !HELPERS_HPP
