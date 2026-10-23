#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <names/grant.hpp>
#include <names/requests.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::ipc::Message;
using cock::user::CallResult;
using cock::user::CallStatus;

namespace {

void discardMessages() {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE) {
	}
}

} // namespace

extern "C" int main() {
	cock::names::waitForCapabilities();
	printf("names: ready\n");

	CallResult result = cock::user::ipc_accept();
	for (;;) {
		if (result.status == CallStatus::MESSAGE_PENDING) {
			discardMessages();
			result = cock::user::ipc_accept();
			continue;
		}
		if (!result.ok) {
			result = cock::user::ipc_accept();
			continue;
		}

		uint32_t reply[cock::user::CALL_WORDS] = {};
		cock::names::serve(result.peer, result.words, reply);
		result = cock::user::ipc_reply_wait(reply);
	}
}
