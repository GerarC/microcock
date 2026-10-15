#include <cock/abi/capability.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

using cock::abi::CapabilityType;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace {

constexpr const char *VGA_DRIVER = "vga_driver";
constexpr const char *KEYBOARD_DRIVER = "kbd";
constexpr uintptr_t VIDEO_WINDOW_START = 0xA0000;
constexpr uintptr_t VIDEO_WINDOW_SIZE = 0x20000;
constexpr uintptr_t PS2_DATA_PORT = 0x60;
constexpr uintptr_t KEYBOARD_IRQ_LINE = 1;
constexpr uintptr_t SINGLE_RESOURCE = 1;
constexpr uintptr_t EXIT_SUCCESS_STATUS = 0;

void announce(uint32_t pid, uint32_t peer) {
	Message message = {};
	message.type = MessageType::CAPABILITY_GRANTED;
	message.payload.args.arg0 = peer;
	cock::user::ipc_send(pid, message);
}

} // namespace

extern "C" void _start() {
	uint32_t display = cock::user::lookup(VGA_DRIVER);
	uint32_t keyboard = cock::user::lookup(KEYBOARD_DRIVER);
	uint32_t files = cock::user::lookup(cock::abi::fs::SERVER_NAME);
	uint32_t initrd = cock::user::module_index(cock::abi::fs::INITRD_MODULE_NAME);

	if (files != NO_PROCESS && initrd != cock::user::NO_MODULE) {
		cock::user::cap_grant(files, CapabilityType::BOOT_MODULE, initrd,
							  SINGLE_RESOURCE);
		announce(files, NO_PROCESS);
	}

	if (display != NO_PROCESS) {
		cock::user::cap_grant(display, CapabilityType::PHYSICAL_MEMORY,
							  VIDEO_WINDOW_START, VIDEO_WINDOW_SIZE);
		announce(display, display);
	}

	if (keyboard != NO_PROCESS) {
		cock::user::cap_grant(keyboard, CapabilityType::IO_PORTS, PS2_DATA_PORT,
							  SINGLE_RESOURCE);
		cock::user::cap_grant(keyboard, CapabilityType::IRQ, KEYBOARD_IRQ_LINE,
							  SINGLE_RESOURCE);
		announce(keyboard, display);
	}

	cock::user::exit_process(EXIT_SUCCESS_STATUS);
}
