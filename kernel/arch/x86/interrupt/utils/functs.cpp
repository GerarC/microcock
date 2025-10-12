#include "functs.hpp"

namespace cock::arch::x86::utils {

void out_port_b(uint16_t port, uint8_t value) {
	asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

} // namespace arch::x86::utils
