#include <cock/abi/ansi.hpp>
#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <console/serial.hpp>
#include <stdint.h>

namespace cock::console {

namespace {

constexpr uint16_t SERIAL_BASE_PORT = 0x3F8;
constexpr uint32_t SERIAL_PORT_COUNT = 8;
constexpr uint16_t SERIAL_LINE_STATUS = SERIAL_BASE_PORT + 5;
constexpr uint8_t SERIAL_TRANSMIT_EMPTY = 0x20;
constexpr uint32_t SERIAL_SPIN_LIMIT = 100000;

cock::abi::ansi::SgrParser parsers[cock::abi::console::TERMINAL_COUNT];

} // namespace

bool openSerial() { return cock::user::request_ports(SERIAL_BASE_PORT, SERIAL_PORT_COUNT); }

void serialWrite(const char *text, size_t length, uint32_t terminal) {
	cock::abi::ansi::SgrParser &parser = parsers[terminal < cock::abi::console::TERMINAL_COUNT ? terminal : cock::abi::console::FIRST_TERMINAL];
	for (size_t index = 0; index < length; index++) {
		if (parser.feed(text[index]) != cock::abi::ansi::Result::PRINT) continue;

		for (uint32_t spin = 0; spin < SERIAL_SPIN_LIMIT; spin++) {
			if (cock::user::port_in8(SERIAL_LINE_STATUS) & SERIAL_TRANSMIT_EMPTY) break;
		}
		cock::user::port_out8(SERIAL_BASE_PORT, static_cast<uint8_t>(text[index]));
	}
}

} // namespace cock::console
