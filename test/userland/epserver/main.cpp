#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/crt0.hpp>
#include <cock/user/endpoint_script.hpp>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::abi::endpoint::MAX_REPLIES_PER_REQUESTER;
using cock::abi::endpoint::NO_BADGE;
using cock::abi::endpoint::NO_REPLY;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::CallResult;
using cock::user::CallStatus;
using namespace cock::user::endpoint_script;

namespace {

constexpr uint32_t NO_ENDPOINT_WORD = 0;
constexpr uint32_t NO_WORD = 0;
constexpr uint32_t NO_HANDLE = 0;

struct Seen {
	bool throughHandle = false;
	bool byProcessId = false;
	bool wrongBadge = false;
	uint32_t forwardHandle = NO_HANDLE;
	bool requested = false;
	bool forwarded = false;
	bool answered = false;
	bool spentTokenRefused = false;
	bool problem = false;
	uint32_t unanswered = 0;
};

Message pingWith(uint32_t tag, uint32_t value) {
	Message message = {};
	message.type = MessageType::SYS_PING;
	message.payload.args.arg0 = tag;
	message.payload.args.arg1 = value;
	return message;
}

void takeRequest(Seen &seen, const Message &message) {
	seen.requested = true;
	if (message.reply == NO_REPLY || message.badge != HANDLE_BADGE ||
		seen.forwardHandle == NO_HANDLE) {
		seen.problem = true;
		return;
	}
	seen.forwarded = cock::user::ep_forward(message.reply, seen.forwardHandle,
											pingWith(TAG_FORWARDED, message.payload.args.arg1));
	if (!seen.forwarded) seen.problem = true;
}

void answerForwarded(Seen &seen, const Message &message) {
	if (message.reply == NO_REPLY || message.badge != FORWARD_BADGE) {
		seen.problem = true;
		return;
	}
	Message answer = pingWith(TAG_ANSWER, message.payload.args.arg1 + ANSWER_INCREMENT);
	seen.answered = cock::user::ep_reply(message.reply, answer);
	seen.spentTokenRefused = !cock::user::ep_reply(message.reply, answer) &&
							 !cock::user::ep_forward(message.reply, seen.forwardHandle, answer);
	if (!seen.answered || !seen.spentTokenRefused) seen.problem = true;
}

void record(Seen &seen, const Message &message) {
	if (message.type != MessageType::SYS_PING) return;
	uint32_t tag = message.payload.args.arg0;
	if (tag == TAG_THROUGH_HANDLE) {
		seen.throughHandle = true;
		seen.wrongBadge = seen.wrongBadge || message.badge != HANDLE_BADGE ||
						  message.reply != NO_REPLY;
	} else if (tag == TAG_BY_PROCESS_ID) {
		seen.byProcessId = true;
	} else if (tag == TAG_REQUEST) {
		takeRequest(seen, message);
	} else if (tag == TAG_FORWARDED) {
		answerForwarded(seen, message);
	} else if (tag == TAG_UNANSWERED) {
		seen.unanswered++;
		if (message.reply == NO_REPLY) seen.problem = true;
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
		if (guess == cock::user::initialHandle()) continue;
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

bool repliesWereHandledAsExpected(const Seen &seen) {
	return seen.requested && seen.forwarded && seen.answered && seen.spentTokenRefused &&
		   !seen.problem && seen.unanswered == MAX_REPLIES_PER_REQUESTER;
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
		} else if (stage == STAGE_SET_FORWARD) {
			seen.forwardHandle = request.words[1];
		} else if (stage == STAGE_FINISH) {
			drain(seen);
			if (!seen.throughHandle || seen.byProcessId || seen.wrongBadge ||
				!repliesWereHandledAsExpected(seen))
				verdict = ANSWER_WRONG;
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
