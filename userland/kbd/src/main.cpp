#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <kbd/console_link.hpp>
#include <kbd/controller.hpp>
#include <kbd/grant.hpp>
#include <kbd/keymap.hpp>
#include <stdio.h>

using cock::ipc::Message;

namespace {

constexpr uintptr_t FAILED_STATUS = 1;
constexpr const char READY_MESSAGE[] = "kbd: ready\n";
constexpr const char FAILED_MESSAGE[] = "kbd: cannot bind the keyboard\n";

} // namespace

extern "C" void _start() {
	uint32_t console = cock::kbd::waitForCapabilities();

	if (!cock::kbd::openController()) {
		puts(FAILED_MESSAGE);
		cock::user::exit_process(FAILED_STATUS);
	}
	puts(READY_MESSAGE);

	cock::kbd::Decoder decoder;
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (!cock::kbd::isInterrupt(message)) continue;

		cock::kbd::sendKey(console, decoder.feed(cock::kbd::readScancode()));
		cock::kbd::acknowledge();
	}
}
