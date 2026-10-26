#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <tty/input.hpp>
#include <tty/output.hpp>
#include <tty/terminal.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

constexpr uint32_t NO_SWITCH = 0;
constexpr uint32_t NO_SWITCH_OFFSET = 1;

uint32_t keyboard = NO_PROCESS;

} // namespace

void handleMessage(const Message &message) {
	if (message.type != MessageType::CONSOLE_KEY) return;

	if (message.badge != keyboard)
		keyboard = cock::user::names::server(proto::KEYBOARD_NAME);
	if (message.badge != keyboard) return;

	uint32_t switchTo = message.payload.args.arg1;
	if (switchTo != NO_SWITCH) return activate(switchTo - NO_SWITCH_OFFSET);

	uint32_t key = message.payload.args.arg0;
	uint32_t terminal = activeTerminal();
	cock::abi::console::LineDiscipline &discipline = terminals[terminal].discipline;
	proto::Echo kind = discipline.feed(key);
	if (kind == proto::Echo::REPLACE)
		return redraw(discipline.replacedLength(), discipline.currentLine(), discipline.currentLength(), terminal);
	echo(kind, static_cast<char>(key), terminal);
}

void drainMessages() {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE)
		handleMessage(message);
}

} // namespace cock::tty
