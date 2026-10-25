#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <pipe/ends.hpp>
#include <pipe/inbox.hpp>
#include <pipe/limits.hpp>
#include <pipe/requests.hpp>
#include <pipe/waiters.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::user::CallResult;
using cock::user::CallStatus;

namespace {

uint32_t waitTime() {
	return cock::pipe::anyEnd() ? cock::pipe::SWEEP_MS : cock::user::CALL_NO_TIMEOUT;
}

} // namespace

extern "C" int main() {
	puts("pipe: ready");

	CallResult result = cock::user::ipc_accept(waitTime());
	for (;;) {
		if (result.status == CallStatus::MESSAGE_PENDING) {
			cock::pipe::discardMessages();
		} else if (!result.ok) {
			if (result.status == CallStatus::TIMED_OUT) cock::pipe::sweepDead();
		} else {
			uint32_t reply[cock::pipe::CALL_WORDS] = {};
			if (cock::pipe::serve(result.peer, result.words, reply) == cock::pipe::Served::REPLY)
				cock::user::ipc_reply(reply);
			cock::pipe::retryWaiters();
		}
		result = cock::user::ipc_accept(waitTime());
	}
}
