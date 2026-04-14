#ifndef ARCH_X86_HELPERS_HPP
#define ARCH_X86_HELPERS_HPP

#include <stdint.h>

namespace cock::arch::x86::utils {

/** @brief The virtual address offset where the Higher-Half kernel is mapped. */
constexpr uintptr_t KERNEL_START = 0xC0000000;

/**
 * @brief Structure used to capture the CPU context (registers) during an interrupt.
 * * Pushed to the stack by the ISR/IRQ assembly stubs before calling C/C++ handlers.
 */
typedef struct _interrupt_registers_t {
    uint32_t cr2;               /**< Control Register 2 (Contains faulting address on Page Faults). */
    uint32_t ds;                /**< Data Segment selector. */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /**< Pushed by pusha instruction. */
    uint32_t int_no, err_code;  /**< Interrupt number and associated error code (if any). */
    uint32_t eip, cs, eflags, useresp, ss; /**< Pushed automatically by the processor. */
} InterruptRegisters;

/**
 * @brief Dumps the captured CPU registers to the standard output for debugging.
 * @param r Pointer to the interrupt registers structure.
 */
void print_interrupt_registers(const InterruptRegisters *r);


/**
 * @brief Writes a single byte to an x86 hardware I/O port.
 * @param port The 16-bit port number.
 * @param value The 8-bit value to write.
 */
void out_port_b(uint16_t port, uint8_t value);

/**
 * @brief Reads a single byte from an x86 hardware I/O port.
 * @param port The 16-bit port number.
 * @return The 8-bit value read from the port.
 */
uint8_t in_port_b(uint16_t port);

/**
 * @brief Performs ceiling division for integers.
 */
uint32_t ceil_div(uint32_t a, uint32_t b);

/**
 * @brief Reads the value of Control Register 2 (CR2).
 * * Typically used inside page fault handlers to identify which address caused the fault.
 * @return The virtual address that caused the page fault.
 */
static inline uint32_t read_cr2() {
    uint32_t val;
    asm volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

}


#endif // !HELPERS_HPP
