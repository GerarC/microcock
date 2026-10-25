#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/endpoint_script.hpp>
#include <cock/user/names.hpp>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::abi::CapabilityType;
using cock::abi::NO_CAPABILITY_NODE;
using cock::abi::endpoint::NO_BADGE;
using cock::abi::endpoint::NO_ENDPOINT;
using cock::abi::endpoint::SINGLE_ENDPOINT;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::CallResult;
using cock::user::CallStatus;
using cock::user::NO_PROCESS;
using namespace cock::user::endpoint_script;

namespace {

constexpr uint32_t NO_WORD = 0;
constexpr uint32_t FORGED_REPLY = 0x1234;
constexpr bool NO_DELEGATE = false;
constexpr bool NO_MINT = false;

bool waitForGrant() {
	Message message = {};
	do {
		if (!cock::user::ipc_recv(message)) return false;
	} while (message.type != MessageType::CAPABILITY_GRANTED);
	return true;
}

uint32_t findServer() {
	for (uint32_t attempt = 0; attempt < WAIT_ROUNDS; attempt++) {
		uint32_t server = cock::user::names::server(SERVER_NAME);
		if (server != NO_PROCESS) return server;
		cock::user::sleep_ms(ATTEMPT_MILLISECONDS);
	}
	return NO_PROCESS;
}

Message pingWith(uint32_t tag, uint32_t forgedBadge, uint32_t value = NO_WORD) {
	Message message = {};
	message.type = MessageType::SYS_PING;
	message.badge = forgedBadge;
	message.reply = FORGED_REPLY;
	message.payload.args.arg0 = tag;
	message.payload.args.arg1 = value;
	return message;
}

bool answeredOk(const CallResult &answer) {
	return answer.ok && answer.words[0] == ANSWER_OK;
}

bool callThroughTheHandle(uint32_t handle) {
	const uint32_t words[cock::user::CALL_WORDS] = {STAGE_CALL, FIRST_PAYLOAD, SECOND_PAYLOAD,
													THIRD_PAYLOAD};
	return answeredOk(cock::user::ep_call(handle, words));
}

bool serverStagesPass(uint32_t server, uint32_t stage, uint32_t endpoint) {
	const uint32_t words[cock::user::CALL_WORDS] = {stage, endpoint, NO_WORD, NO_WORD};
	return answeredOk(cock::user::ipc_call(server, words));
}

bool unheldHandlesFail() {
	const uint32_t words[cock::user::CALL_WORDS] = {NO_WORD, NO_WORD, NO_WORD, NO_WORD};
	Message message = pingWith(TAG_THROUGH_HANDLE, NO_BADGE);
	return !cock::user::ep_send(UNHELD_HANDLE, message) &&
		   cock::user::ep_call(UNHELD_HANDLE, words, FAILING_CALL_MILLISECONDS).status ==
			   CallStatus::FAILED &&
		   !cock::user::ep_send(NO_CAPABILITY_NODE, message);
}

bool serverHoldsAForwardHandle(uint32_t server, uint32_t endpoint) {
	uint32_t forward = cock::user::cap_grant_node(server, CapabilityType::ENDPOINT, endpoint,
												  SINGLE_ENDPOINT, NO_DELEGATE, NO_MINT, FORWARD_BADGE);
	return forward != NO_CAPABILITY_NODE && serverStagesPass(server, STAGE_SET_FORWARD, forward);
}

bool requestIsAnsweredThroughItsToken(uint32_t handle, uint32_t server) {
	if (!cock::user::ep_send(handle, pingWith(TAG_REQUEST, FORGED_BADGE, REQUEST_VALUE), true))
		return false;

	Message answer = {};
	for (uint32_t round = 0; round < WAIT_ROUNDS; round++) {
		cock::user::Received received = cock::user::ipc_recv_timeout(answer, ANSWER_WAIT_MILLISECONDS);
		if (received == cock::user::Received::FAILED) return false;
		if (received == cock::user::Received::EMPTY) continue;
		return answer.type == MessageType::SYS_PING && answer.sender_pid == server &&
			   answer.payload.args.arg0 == TAG_ANSWER &&
			   answer.payload.args.arg1 == REQUEST_VALUE + ANSWER_INCREMENT &&
			   answer.reply == cock::abi::endpoint::NO_REPLY && answer.badge == NO_BADGE;
	}
	return false;
}

bool tokensOfOthersFail(uint32_t handle) {
	Message message = pingWith(TAG_ANSWER, NO_BADGE, NO_WORD);
	for (uint32_t guess = FIRST_TOKEN_GUESS; guess < FIRST_TOKEN_GUESS + TOKEN_GUESS_COUNT; guess++) {
		if (cock::user::ep_reply(guess, message) || cock::user::ep_forward(guess, handle, message))
			return false;
	}
	return !cock::user::ep_reply(cock::abi::endpoint::NO_REPLY, message);
}

bool requesterQuotaHolds(uint32_t handle) {
	for (size_t sent = 0; sent < cock::abi::endpoint::MAX_REPLIES_PER_REQUESTER; sent++) {
		if (!cock::user::ep_send(handle, pingWith(TAG_UNANSWERED, NO_BADGE, NO_WORD), true))
			return false;
	}
	return !cock::user::ep_send(handle, pingWith(TAG_UNANSWERED, NO_BADGE, NO_WORD), true);
}

bool revokedHandleFails(uint32_t handle) {
	const uint32_t words[cock::user::CALL_WORDS] = {NO_WORD, NO_WORD, NO_WORD, NO_WORD};
	Message message = pingWith(TAG_THROUGH_HANDLE, NO_BADGE);
	return cock::user::cap_revoke(handle) && !cock::user::ep_send(handle, message) &&
		   cock::user::ep_call(handle, words, FAILING_CALL_MILLISECONDS).status == CallStatus::FAILED;
}

bool bindingToAnotherProcessMovesTheEndpoint(uint32_t ownerHandle, uint32_t endpoint) {
	if (!cock::user::ep_bind(endpoint, cock::user::self())) return false;
	if (!cock::user::ep_send(ownerHandle, pingWith(TAG_AFTER_REBIND, FORGED_BADGE))) return false;

	Message message = {};
	if (cock::user::ipc_try_recv(message) != cock::user::Received::MESSAGE) return false;
	return message.type == MessageType::SYS_PING &&
		   message.payload.args.arg0 == TAG_AFTER_REBIND && message.badge == NO_BADGE;
}

} // namespace

extern "C" int main() {
	bool passed = waitForGrant();
	uint32_t server = passed ? findServer() : NO_PROCESS;
	passed = passed && server != NO_PROCESS;

	uint32_t ownerHandle = NO_CAPABILITY_NODE;
	uint32_t endpoint = passed ? cock::user::ep_create(ownerHandle) : NO_ENDPOINT;
	passed = passed && endpoint != NO_ENDPOINT && ownerHandle != NO_CAPABILITY_NODE &&
			 cock::user::ep_bind(endpoint, server);
	if (passed) puts("epgiver: endpoint bound to the server");

	uint32_t handle = passed ? cock::user::cap_grant_node(cock::user::self(), CapabilityType::ENDPOINT,
														  endpoint, SINGLE_ENDPOINT, NO_DELEGATE,
														  NO_MINT, HANDLE_BADGE)
							 : NO_CAPABILITY_NODE;
	passed = passed && handle != NO_CAPABILITY_NODE && callThroughTheHandle(handle);
	if (passed) puts("epgiver: the server saw the badge of the handle");

	passed = passed && cock::user::ep_send(handle, pingWith(TAG_THROUGH_HANDLE, FORGED_BADGE)) &&
			 cock::user::ipc_send(server, pingWith(TAG_BY_PROCESS_ID, FORGED_BADGE));
	passed = passed && unheldHandlesFail() && serverStagesPass(server, STAGE_CHECK, endpoint);
	if (passed) puts("epgiver: nothing works without a handle");

	passed = passed && serverHoldsAForwardHandle(server, endpoint) &&
			 requestIsAnsweredThroughItsToken(handle, server);
	if (passed) puts("epgiver: a request was forwarded and answered through its token");

	passed = passed && tokensOfOthersFail(handle);
	if (passed) puts("epgiver: tokens nobody gave and spent tokens fail");

	passed = passed && requesterQuotaHolds(handle);
	if (passed) puts("epgiver: a requester cannot open more tokens than its quota");

	passed = passed && serverStagesPass(server, STAGE_FINISH, endpoint);
	if (passed) puts("epgiver: the kernel replaced the forged badges");

	passed = passed && revokedHandleFails(handle);
	if (passed) puts("epgiver: a revoked handle leads nowhere");

	passed = passed && bindingToAnotherProcessMovesTheEndpoint(ownerHandle, endpoint);
	if (passed) puts("epgiver: binding again moved the endpoint");

	puts(passed ? "epgiver: ok" : "epgiver: failed");
	_exit(passed ? EXIT_PASSED : EXIT_FAILED);
}
