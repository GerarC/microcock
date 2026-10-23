#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <console/display.hpp>
#include <console/grant.hpp>
#include <console/input.hpp>
#include <console/readers.hpp>
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

uint32_t waitTime() {
	return cock::console::hasPartialText() ? SWEEP_MS : cock::user::CALL_NO_TIMEOUT;
}

} // namespace

extern "C" int main() {
	cock::console::setDisplay(cock::console::waitForCapabilities());

	if (!cock::console::openSerial()) {
		puts("console: cannot open the serial port");
		cock::user::exit_process(FAILED_STATUS);
	}
	puts("console: ready");

	CallResult result = cock::user::ipc_accept(waitTime());
	for (;;) {
		if (result.status == CallStatus::MESSAGE_PENDING) {
			cock::console::drainMessages();
			cock::console::wakeReaders();
			result = cock::user::ipc_accept(waitTime());
			continue;
		}
		if (!result.ok) {
			if (result.status == CallStatus::TIMED_OUT) cock::console::sweepDeadWriters();
			result = cock::user::ipc_accept(waitTime());
			continue;
		}

		uint32_t reply[cock::console::ANSWER_WORDS] = {};
		if (cock::console::serve(result.peer, result.words, reply) ==
			cock::console::Served::DEFERRED) {
			result = cock::user::ipc_accept(waitTime());
			continue;
		}
		result = cock::user::ipc_reply_wait(reply, waitTime());
	}
}
