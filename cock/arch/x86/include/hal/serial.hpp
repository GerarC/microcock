#ifndef ARCH_X86_HAL_SERIAL_HPP
#define ARCH_X86_HAL_SERIAL_HPP

/**
 * @file
 * @brief HAL: polled 16550 UART on COM1, the primary kernel log sink.
 *
 * Port requirement: provide serial_init() and serial_write(char). A port may
 * back them with any byte oriented debug UART or semihosting channel. The
 * constants in this file describe the x86 COM1 register layout and are private
 * to the x86 port.
 */

#include <hal/cpu.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::hal {

/** @brief I/O base port of COM1. */
constexpr uint16_t SERIAL_BASE_PORT = 0x3F8;
/** @brief Transmit/receive data register (offset 0, divisor low when DLAB). */
constexpr uint16_t SERIAL_DATA = SERIAL_BASE_PORT;
/** @brief Interrupt enable register (offset 1). */
constexpr uint16_t SERIAL_INTERRUPT_ENABLE = SERIAL_BASE_PORT + 1;
/** @brief Divisor latch high byte (offset 1 while DLAB is set). */
constexpr uint16_t SERIAL_DIVISOR_HIGH = SERIAL_BASE_PORT + 1;
/** @brief FIFO control register (offset 2). */
constexpr uint16_t SERIAL_FIFO_CONTROL = SERIAL_BASE_PORT + 2;
/** @brief Line control register (offset 3, bit 7 is DLAB). */
constexpr uint16_t SERIAL_LINE_CONTROL = SERIAL_BASE_PORT + 3;
/** @brief Modem control register (offset 4). */
constexpr uint16_t SERIAL_MODEM_CONTROL = SERIAL_BASE_PORT + 4;
/** @brief Line status register (offset 5). */
constexpr uint16_t SERIAL_LINE_STATUS = SERIAL_BASE_PORT + 5;

/** @brief Interrupt enable value that disables every UART interrupt. */
constexpr uint8_t SERIAL_INTERRUPTS_OFF = 0x00;
/** @brief Line control bit 7 (DLAB): exposes the baud divisor registers. */
constexpr uint8_t SERIAL_DIVISOR_LATCH = 0x80;
/** @brief Divisor low byte for 115200 baud (divisor 1). */
constexpr uint8_t SERIAL_DIVISOR_LOW_BAUD_115200 = 0x01;
/** @brief Divisor high byte for 115200 baud (divisor 1). */
constexpr uint8_t SERIAL_DIVISOR_HIGH_BAUD_115200 = 0x00;
/** @brief Line control value for 8 data bits, no parity, one stop bit (8N1). */
constexpr uint8_t SERIAL_8_DATA_BITS_NO_PARITY_ONE_STOP = 0x03;
/** @brief FIFO control value: enable FIFOs, clear both, 14 byte trigger. */
constexpr uint8_t SERIAL_FIFO_ENABLE_CLEAR_14_BYTES = 0xC7;
/** @brief Modem control value: assert DTR, RTS and OUT2. */
constexpr uint8_t SERIAL_MODEM_DTR_RTS_OUT2 = 0x0B;
/** @brief Line status bit 5: the transmit holding register is empty. */
constexpr uint8_t SERIAL_TRANSMIT_EMPTY = 0x20;

/**
 * @brief Configures COM1 as 115200 baud, 8N1, FIFO on, interrupts off.
 *
 * Writes the UART registers with `out`. Ring 0 only, once, before the first
 * serial_write().
 */
[[gnu::always_inline]] inline void serial_init() {
	port_out8(SERIAL_INTERRUPT_ENABLE, SERIAL_INTERRUPTS_OFF);
	port_out8(SERIAL_LINE_CONTROL, SERIAL_DIVISOR_LATCH);
	port_out8(SERIAL_DATA, SERIAL_DIVISOR_LOW_BAUD_115200);
	port_out8(SERIAL_DIVISOR_HIGH, SERIAL_DIVISOR_HIGH_BAUD_115200);
	port_out8(SERIAL_LINE_CONTROL, SERIAL_8_DATA_BITS_NO_PARITY_ONE_STOP);
	port_out8(SERIAL_FIFO_CONTROL, SERIAL_FIFO_ENABLE_CLEAR_14_BYTES);
	port_out8(SERIAL_MODEM_CONTROL, SERIAL_MODEM_DTR_RTS_OUT2);
}

/**
 * @brief Sends one byte, busy waiting until the transmitter is ready.
 *
 * Spins reading the line status register, then writes the data register. It
 * can spin forever if no UART answers; Ring 0 only.
 *
 * @param c Byte to transmit.
 */
[[gnu::always_inline]] inline void serial_write(char c) {
	while (!(port_in8(SERIAL_LINE_STATUS) & SERIAL_TRANSMIT_EMPTY)) {}
	port_out8(SERIAL_DATA, static_cast<uint8_t>(c));
}

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_SERIAL_HPP
