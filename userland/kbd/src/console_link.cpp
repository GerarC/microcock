#include <cock/abi/console.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <kbd/console_link.hpp>
#include <kbd/keymap.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace cock::kbd {

void sendKey(uint32_t &console, char character) {
	if (character == NO_CHARACTER) return;

	Message message = {};
	message.type = MessageType::CONSOLE_KEY;
	message.payload.args.arg0 = static_cast<uint32_t>(character);
	if (console != NO_PROCESS && cock::user::ipc_send(console, message)) return;

	console = cock::user::names::server(cock::abi::console::SERVER_NAME);
	if (console != NO_PROCESS) cock::user::ipc_send(console, message);
}

} // namespace cock::kbd
