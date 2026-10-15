#ifndef ARCH_X86_HAL_SERIAL_HPP
#define ARCH_X86_HAL_SERIAL_HPP

#include <hal/cpu.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::hal {

constexpr uint16_t SERIAL_BASE_PORT = 0x3F8;
constexpr uint16_t SERIAL_DATA = SERIAL_BASE_PORT;
constexpr uint16_t SERIAL_INTERRUPT_ENABLE = SERIAL_BASE_PORT + 1;
constexpr uint16_t SERIAL_DIVISOR_HIGH = SERIAL_BASE_PORT + 1;
constexpr uint16_t SERIAL_FIFO_CONTROL = SERIAL_BASE_PORT + 2;
constexpr uint16_t SERIAL_LINE_CONTROL = SERIAL_BASE_PORT + 3;
constexpr uint16_t SERIAL_MODEM_CONTROL = SERIAL_BASE_PORT + 4;
constexpr uint16_t SERIAL_LINE_STATUS = SERIAL_BASE_PORT + 5;

constexpr uint8_t SERIAL_INTERRUPTS_OFF = 0x00;
constexpr uint8_t SERIAL_DIVISOR_LATCH = 0x80;
constexpr uint8_t SERIAL_DIVISOR_LOW_BAUD_115200 = 0x01;
constexpr uint8_t SERIAL_DIVISOR_HIGH_BAUD_115200 = 0x00;
constexpr uint8_t SERIAL_8_DATA_BITS_NO_PARITY_ONE_STOP = 0x03;
constexpr uint8_t SERIAL_FIFO_ENABLE_CLEAR_14_BYTES = 0xC7;
constexpr uint8_t SERIAL_MODEM_DTR_RTS_OUT2 = 0x0B;
constexpr uint8_t SERIAL_TRANSMIT_EMPTY = 0x20;

[[gnu::always_inline]] inline void serial_init() {
	port_out8(SERIAL_INTERRUPT_ENABLE, SERIAL_INTERRUPTS_OFF);
	port_out8(SERIAL_LINE_CONTROL, SERIAL_DIVISOR_LATCH);
	port_out8(SERIAL_DATA, SERIAL_DIVISOR_LOW_BAUD_115200);
	port_out8(SERIAL_DIVISOR_HIGH, SERIAL_DIVISOR_HIGH_BAUD_115200);
	port_out8(SERIAL_LINE_CONTROL, SERIAL_8_DATA_BITS_NO_PARITY_ONE_STOP);
	port_out8(SERIAL_FIFO_CONTROL, SERIAL_FIFO_ENABLE_CLEAR_14_BYTES);
	port_out8(SERIAL_MODEM_CONTROL, SERIAL_MODEM_DTR_RTS_OUT2);
}

[[gnu::always_inline]] inline void serial_write(char c) {
	while (!(port_in8(SERIAL_LINE_STATUS) & SERIAL_TRANSMIT_EMPTY)) {}
	port_out8(SERIAL_DATA, static_cast<uint8_t>(c));
}

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_SERIAL_HPP
