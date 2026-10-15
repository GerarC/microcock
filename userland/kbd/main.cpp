#include "keymap.hpp"
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdio.h>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace {

constexpr uint16_t PS2_DATA_PORT = 0x60;
constexpr uint32_t PS2_PORT_COUNT = 1;
constexpr uint32_t KEYBOARD_IRQ_LINE = 1;
constexpr uintptr_t FAILED_STATUS = 1;
constexpr const char READY_MESSAGE[] = "kbd: ready\n";
constexpr const char FAILED_MESSAGE[] = "kbd: cannot bind the keyboard\n";

uint32_t waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED)
			return message.payload.args.arg0;
	}
}

void show(uint32_t display, char character) {
	if (display == NO_PROCESS || character == kbd::NO_CHARACTER) return;

	Message message = {};
	message.type = MessageType::VIDEO_WRITE_CHAR;
	message.payload.args.arg0 = static_cast<uint32_t>(character);
	cock::user::ipc_send(display, message);
}

} // namespace

extern "C" void _start() {
	uint32_t display = waitForCapabilities();

	if (!cock::user::request_ports(PS2_DATA_PORT, PS2_PORT_COUNT) ||
		!cock::user::irq_bind(KEYBOARD_IRQ_LINE)) {
		puts(FAILED_MESSAGE);
		cock::user::exit_process(FAILED_STATUS);
	}
	puts(READY_MESSAGE);

	kbd::Decoder decoder;
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type != MessageType::IRQ_NOTIFICATION ||
		message.sender_pid != cock::ipc::KERNEL_SENDER)
		continue;

		show(display, decoder.feed(cock::user::port_in8(PS2_DATA_PORT)));
		cock::user::irq_ack(KEYBOARD_IRQ_LINE);
	}
}
