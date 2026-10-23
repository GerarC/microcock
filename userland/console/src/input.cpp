#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <console/display.hpp>
#include <console/input.hpp>

using cock::abi::console::Echo;
using cock::abi::console::LineDiscipline;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace proto = cock::abi::console;

namespace cock::console {

namespace {

uint32_t keyboard = NO_PROCESS;

void echo(Echo kind, char character) {
	switch (kind) {
	case Echo::NONE:
		return;
	case Echo::CHARACTER:
		return screenCharacter(character);
	case Echo::ERASE:
		return screenCharacter(proto::CHAR_BACKSPACE);
	case Echo::NEWLINE:
		return screenCharacter(proto::CHAR_NEWLINE);
	}
}

} // namespace

LineDiscipline discipline;

void handleMessage(const Message &message) {
	if (message.type != MessageType::CONSOLE_KEY) return;
	if (message.sender_pid == cock::ipc::KERNEL_SENDER) return;

	if (message.sender_pid != keyboard)
		keyboard = cock::user::names::server(proto::KEYBOARD_NAME);
	if (message.sender_pid != keyboard) return;

	char character = static_cast<char>(message.payload.args.arg0);
	echo(discipline.feed(character), character);
}

void drainMessages() {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE)
		handleMessage(message);
}

} // namespace cock::console
