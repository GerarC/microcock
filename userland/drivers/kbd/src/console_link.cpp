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

void sendKey(uint32_t &console, char character) {
	if (character == NO_CHARACTER) return;

	Message message = {};
	message.type = MessageType::CONSOLE_KEY;
	message.payload.args.arg0 = static_cast<uint32_t>(character);
	if (console != NO_CAPABILITY_NODE && cock::user::ep_send(console, message)) return;

	console = cock::user::names::connect(cock::abi::console::TTY_NAME).handle;
	if (console != NO_CAPABILITY_NODE) cock::user::ep_send(console, message);
}

} // namespace cock::kbd
