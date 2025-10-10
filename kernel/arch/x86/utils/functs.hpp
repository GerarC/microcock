#ifndef FUNCTS_HPP
#define FUNCTS_HPP

#include <stdint.h>

namespace arch::x86::utils {

typedef struct _interrupt_registers_t {
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, csm_eflags, userresp, ss;
} InterruptRegisters;

/**
 * Outputs a byte to the given port
 * */
void out_port_b(uint16_t port, uint8_t value);

}

#endif // !FUNCTS_HPP
