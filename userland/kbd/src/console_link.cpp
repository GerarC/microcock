#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <kbd/console_link.hpp>
#include <kbd/keymap.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace cock::kbd {

void sendKey(uint32_t console, char character) {
	if (console == NO_PROCESS || character == NO_CHARACTER) return;

	Message message = {};
	message.type = MessageType::CONSOLE_KEY;
	message.payload.args.arg0 = static_cast<uint32_t>(character);
	cock::user::ipc_send(console, message);
}

} // namespace cock::kbd
