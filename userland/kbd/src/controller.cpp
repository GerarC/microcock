#include <cock/user/api.hpp>
#include <kbd/controller.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::kbd {

namespace {

constexpr uint16_t PS2_DATA_PORT = 0x60;
constexpr uint32_t PS2_PORT_COUNT = 1;
constexpr uint32_t KEYBOARD_IRQ_LINE = 1;

} // namespace

bool openController() {
	return cock::user::request_ports(PS2_DATA_PORT, PS2_PORT_COUNT) &&
		   cock::user::irq_bind(KEYBOARD_IRQ_LINE);
}

bool isInterrupt(const Message &message) {
	return message.type == MessageType::IRQ_NOTIFICATION &&
		   message.sender_pid == cock::ipc::KERNEL_SENDER;
}

uint8_t readScancode() { return cock::user::port_in8(PS2_DATA_PORT); }

void acknowledge() { cock::user::irq_ack(KEYBOARD_IRQ_LINE); }

} // namespace cock::kbd
