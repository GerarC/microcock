#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <console/display.hpp>
#include <console/grant.hpp>
#include <console/requests.hpp>
#include <console/serial.hpp>
#include <console/writers.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::user::CallResult;
using cock::user::CallStatus;

namespace {

constexpr uintptr_t FAILED_STATUS = 1;
constexpr uint32_t SWEEP_MS = 50;
constexpr const char NO_DISPLAY_MESSAGE[] = "console: no display server";

void dropMessages() {
	cock::ipc::Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE) {
	}
}

uint32_t waitTime() {
	return cock::console::hasPartialText() ? SWEEP_MS : cock::user::CALL_NO_TIMEOUT;
}

} // namespace

extern "C" int main() {
	cock::console::waitForCapabilities();
	if (!cock::console::connectDisplay()) puts(NO_DISPLAY_MESSAGE);

	if (!cock::console::openSerial()) {
		puts("console: cannot open the serial port");
		cock::user::exit_process(FAILED_STATUS);
	}
	puts("console: ready");

	CallResult result = cock::user::ipc_accept(waitTime());
	for (;;) {
		if (result.status == CallStatus::MESSAGE_PENDING) {
			dropMessages();
			result = cock::user::ipc_accept(waitTime());
			continue;
		}
		if (!result.ok) {
			if (result.status == CallStatus::TIMED_OUT) cock::console::sweepDeadWriters();
			result = cock::user::ipc_accept(waitTime());
			continue;
		}

		uint32_t reply[cock::console::ANSWER_WORDS] = {};
		cock::console::serve(result.peer, result.words, reply);
		result = cock::user::ipc_reply_wait(reply, waitTime());
	}
}
