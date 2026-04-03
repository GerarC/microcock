#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/driver/serial.hpp>
#include <stdint.h>

namespace cock::driver {

using arch::x86::utils::in_port_b;
using arch::x86::utils::out_port_b;

constexpr uint16_t COM1 = 0x3F8;
static constexpr uint8_t DEACTIVATE_INT = 0x00;
static constexpr uint8_t ENABLE_DLAB = 0x80;
static constexpr uint8_t THREE_DIVISOR_LO = 0x03;
static constexpr uint8_t THREE_DIVISOR_HI = 0x00;
static constexpr uint8_t PARITY_BIT = 0x03;
static constexpr uint8_t ENABLE_FIFO = 0xC7;
static constexpr uint8_t ENABLE_IRQ = 0x0B;

void Serial::init() {
	out_port_b(COM1 + 1, DEACTIVATE_INT);
	out_port_b(COM1 + 3, ENABLE_DLAB);
	out_port_b(COM1 + 0, THREE_DIVISOR_LO);
	out_port_b(COM1 + 1, THREE_DIVISOR_HI);
	out_port_b(COM1 + 3, PARITY_BIT);
	out_port_b(COM1 + 2, ENABLE_FIFO);
	out_port_b(COM1 + 4, ENABLE_IRQ);
}

int Serial::isTransmitEmpty() { return in_port_b(COM1 + 5) & 0x20; }

void Serial::putChar(char c) {
	while (isTransmitEmpty() == 0)
		;
	out_port_b(COM1, static_cast<uint8_t>(c));
}

void Serial::write(const char *data, size_t size) {
	for (size_t i = 0; i < size; i++) {
		putChar(data[i]);
	}
}

void Serial::writeString(const char *data) {
	for (size_t i = 0; data[i] != '\0'; i++) {
		putChar(data[i]);
	}
}

} // namespace cock::driver
