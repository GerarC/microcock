#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <tty/input.hpp>
#include <tty/output.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

uint32_t keyboard = NO_PROCESS;

} // namespace

cock::abi::console::LineDiscipline discipline;

void handleMessage(const Message &message) {
	if (message.type != MessageType::CONSOLE_KEY) return;

	if (message.badge != keyboard)
		keyboard = cock::user::names::server(proto::KEYBOARD_NAME);
	if (message.badge != keyboard) return;

	char character = static_cast<char>(message.payload.args.arg0);
	echo(discipline.feed(character), character);
}

void drainMessages() {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE)
		handleMessage(message);
}

} // namespace cock::tty
