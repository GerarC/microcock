#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <stdint.h>
#include <stdio.h>
#include <tty/input.hpp>
#include <tty/readers.hpp>
#include <tty/requests.hpp>

using cock::user::CallResult;
using cock::user::CallStatus;

extern "C" int main() {
	puts("tty: ready");

	CallResult result = cock::user::ipc_accept(cock::user::CALL_NO_TIMEOUT);
	for (;;) {
		if (result.status == CallStatus::MESSAGE_PENDING) {
			cock::tty::drainMessages();
			cock::tty::wakeReaders();
			result = cock::user::ipc_accept(cock::user::CALL_NO_TIMEOUT);
			continue;
		}
		if (!result.ok) {
			result = cock::user::ipc_accept(cock::user::CALL_NO_TIMEOUT);
			continue;
		}

		uint32_t reply[cock::tty::ANSWER_WORDS] = {};
		if (cock::tty::serve(result.peer, result.words, reply) == cock::tty::Served::DEFERRED) {
			result = cock::user::ipc_accept(cock::user::CALL_NO_TIMEOUT);
			continue;
		}
		result = cock::user::ipc_reply_wait(reply, cock::user::CALL_NO_TIMEOUT);
	}
}
