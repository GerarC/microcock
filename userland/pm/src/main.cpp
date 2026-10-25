#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <pm/exit_notice.hpp>
#include <pm/fork.hpp>
#include <pm/grant.hpp>
#include <pm/requests.hpp>
#include <pm/state.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::ipc::Message;
using cock::user::CallResult;
using cock::user::CallStatus;

namespace {

void takeMessages() {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE)
		cock::pm::takeMessage(message);
}

uint32_t waitLimit(bool pending) {
	return pending ? cock::pm::NOTICE_RETRY_MILLISECONDS : cock::user::CALL_NO_TIMEOUT;
}

} // namespace

extern "C" int main() {
	cock::pm::Grant grant = cock::pm::waitForCapabilities();
	cock::pm::rootHandle = grant.rootHandle;
	cock::pm::namesEndpoint = grant.namesEndpoint;
	printf("pm: ready\n");

	bool pending = false;
	CallResult result = cock::user::ipc_accept(waitLimit(pending));
	for (;;) {
		if (result.status == CallStatus::MESSAGE_PENDING) takeMessages();
		if (result.ok) {
			uint32_t reply[cock::user::CALL_WORDS] = {};
			bool answered = cock::pm::serve(result.peer, result.words, reply);
			pending = cock::pm::flushNotices();
			result = answered ? cock::user::ipc_reply_wait(reply, waitLimit(pending))
							  : cock::user::ipc_accept(waitLimit(pending));
			continue;
		}
		if (result.status == CallStatus::TIMED_OUT) cock::pm::ageSettling();
		pending = cock::pm::flushNotices();
		result = cock::user::ipc_accept(waitLimit(pending));
	}
}
