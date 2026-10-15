#ifndef ARCH_X86_HELPERS_HPP
#define ARCH_X86_HELPERS_HPP

#include <stdint.h>

namespace cock::arch::x86::utils {

/**
 * Structure to capture Registers on Interruption triggering
 * */
typedef struct _interrupt_registers_t {
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} InterruptRegisters;


void print_interrupt_registers(const InterruptRegisters *r);


/**
 * Outputs a byte to the given port
 * */
void out_port_b(uint16_t port, uint8_t value);


/**
 * Reads a byte from the given port
 * */
uint8_t in_port_b(uint16_t port);

uint32_t ceil_div(uint32_t a, uint32_t b);

static inline uint32_t read_cr2() {
    uint32_t val;
    asm volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint32_t read_cr3() {
    uint32_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void invalidate_page(uintptr_t address) {
    asm volatile("invlpg (%0)" ::"r"(address) : "memory");
}

static inline void io_wait() {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

}


#endif // !HELPERS_HPP
