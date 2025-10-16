#include "helpers.hpp"
#include <stdio.h>

namespace cock::arch::x86::utils {

void out_port_b(uint16_t port, uint8_t value) {
	asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t in_port_b(uint16_t port) {
	uint8_t read_value;
	asm volatile("inb %1, %0" : "=a"(read_value) : "dN"(port));
    return read_value;
}

void print_interrupt_registers(const InterruptRegisters *r) {
	printf("\n==== INTERRUPT REGISTERS ====\n");
	printf("INT: %u  ERR: 0x%x\n", r->int_no, r->err_code);
	printf("CR2: 0x%x  DS: 0x%x\n", r->cr2, r->ds);

	printf("\n-- General Purpose --\n");
	printf("EAX: 0x%x  EBX: 0x%x  ECX: 0x%x  EDX: 0x%x\n", r->eax, r->ebx,
		   r->ecx, r->edx);
	printf("ESI: 0x%x  EDI: 0x%x  EBP: 0x%x  ESP: 0x%x\n", r->esi, r->edi,
		   r->ebp, r->esp);

	printf("\n-- Segment / Control --\n");
	printf("CS: 0x%x  SS: 0x%x  DS: 0x%x\n", r->cs, r->ss, r->ds);
	printf("EFLAGS: 0x%x  USERESP: 0x%x\n", r->eflags, r->useresp);
	printf("EI: 0x%x\n", r->eip);
	printf("=============================\n");
}

} // namespace cock::arch::x86::utils
