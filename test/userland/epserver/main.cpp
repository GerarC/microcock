#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/endpoint_script.hpp>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::CallResult;
using cock::user::CallStatus;
using namespace cock::user::endpoint_script;

namespace {

constexpr uint32_t NO_ENDPOINT_WORD = 0;
constexpr uint32_t NO_WORD = 0;

struct Seen {
	bool throughHandle = false;
	bool byProcessId = false;
	bool wrongBadge = false;
};

void record(Seen &seen, const Message &message) {
	if (message.type != MessageType::SYS_PING) return;
	if (message.payload.args.arg0 == TAG_THROUGH_HANDLE) {
		seen.throughHandle = true;
		seen.wrongBadge = seen.wrongBadge || message.badge != HANDLE_BADGE;
	} else if (message.payload.args.arg0 == TAG_BY_PROCESS_ID) {
		seen.byProcessId = true;
		seen.wrongBadge = seen.wrongBadge || message.badge != cock::abi::endpoint::NO_BADGE;
	}
}

void drain(Seen &seen) {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE)
		record(seen, message);
}

bool nothingCanBeDoneWithoutAHandle(uint32_t endpoint) {
	uint32_t handle = 0;
	if (cock::user::ep_create(handle) != cock::abi::endpoint::NO_ENDPOINT) return false;
	if (cock::user::ep_bind(endpoint, cock::user::self())) return false;

	Message message = {};
	message.type = MessageType::SYS_PING;
	const uint32_t words[cock::user::CALL_WORDS] = {NO_WORD, NO_WORD, NO_WORD, NO_WORD};
	for (uint32_t guess = FIRST_GUESS; guess < FIRST_GUESS + GUESS_COUNT; guess++) {
		if (cock::user::ep_send(guess, message)) return false;
		if (cock::user::ep_call(guess, words, FAILING_CALL_MILLISECONDS).status != CallStatus::FAILED)
			return false;
	}
	return !cock::user::ep_send(UNHELD_HANDLE, message);
}

bool echoesWithTheBadge(const CallResult &request) {
	return request.peer == HANDLE_BADGE && request.words[1] == FIRST_PAYLOAD &&
		   request.words[2] == SECOND_PAYLOAD && request.words[3] == THIRD_PAYLOAD;
}

} // namespace

extern "C" int main() {
	bool passed = true;
	Seen seen;
	CallResult request = cock::user::ipc_accept();
	for (;;) {
		if (request.status == CallStatus::MESSAGE_PENDING) {
			drain(seen);
			request = cock::user::ipc_accept();
			continue;
		}
		if (!request.ok) {
			passed = false;
			break;
		}

		uint32_t verdict = ANSWER_OK;
		uint32_t stage = request.words[0];
		if (stage == STAGE_CALL) {
			if (!echoesWithTheBadge(request)) verdict = ANSWER_WRONG;
		} else if (stage == STAGE_CHECK) {
			if (!nothingCanBeDoneWithoutAHandle(request.words[1])) verdict = ANSWER_WRONG;
		} else if (stage == STAGE_FINISH) {
			drain(seen);
			if (!seen.throughHandle || !seen.byProcessId || seen.wrongBadge) verdict = ANSWER_WRONG;
		} else {
			verdict = ANSWER_WRONG;
		}
		passed = passed && verdict == ANSWER_OK;

		const uint32_t answer[cock::user::CALL_WORDS] = {verdict, request.peer, NO_WORD, NO_WORD};
		if (stage == STAGE_FINISH) {
			cock::user::ipc_reply(answer);
			break;
		}
		request = cock::user::ipc_reply_wait(answer);
	}

	puts(passed ? "epserver: ok" : "epserver: failed");
	_exit(passed ? EXIT_PASSED : EXIT_FAILED);
}
