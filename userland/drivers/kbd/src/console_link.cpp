#include <cock/abi/console.hpp>
#include <cock/abi/capability.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <kbd/console_link.hpp>
#include <kbd/keymap.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::abi::NO_CAPABILITY_NODE;

namespace cock::kbd {

namespace {

void deliver(uint32_t &console, const Message &message) {
	if (console != NO_CAPABILITY_NODE && cock::user::ep_send(console, message)) return;

	console = cock::user::names::connect(cock::abi::console::TTY_NAME).handle;
	if (console != NO_CAPABILITY_NODE) cock::user::ep_send(console, message);
}

} // namespace

void sendKey(uint32_t &console, uint32_t key) {
	if (key == NO_KEY) return;

	Message message = {};
	message.type = MessageType::CONSOLE_KEY;
	message.payload.args.arg0 = key;
	deliver(console, message);
}

void sendSwitch(uint32_t &console, uint32_t terminal) {
	Message message = {};
	message.type = MessageType::CONSOLE_KEY;
	message.payload.args.arg1 = terminal;
	deliver(console, message);
}

} // namespace cock::kbd
