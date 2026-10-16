#include "probe.hpp"
#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>

using cock::user::CALL_WORDS;
using cock::user::CallResult;
using cock::user::NO_PROCESS;

namespace probe {

namespace {

constexpr size_t STACK_BYTES = 0x4000;
constexpr size_t STACK_COUNT = 3;
constexpr uint32_t ROUNDS = 50;
constexpr uint32_t STOP_WORD = 0xDEAD;
constexpr uint32_t SETTLE_MILLISECONDS = 60;
constexpr uint32_t QUEUE_MILLISECONDS = 100;
constexpr uint32_t ORDER_MILLISECONDS = 20;
constexpr uint32_t UNKNOWN_THREAD = 0x7FFFFFF0;
constexpr uint32_t FIRST_MARKER = 0x1111;
constexpr uint32_t SECOND_MARKER = 0x2222;
constexpr size_t FIRST_STACK = 0;
constexpr size_t SECOND_STACK = 1;
constexpr size_t SERVER_STACK = 2;
constexpr uint32_t FIRST_SEQUENCE = 1;
constexpr uint32_t SECOND_SEQUENCE = 2;
constexpr uint32_t CALLER_COUNT = 2;
constexpr uint32_t CALL_DEADLINE_MILLISECONDS = 50;
constexpr uint32_t SLOW_ANSWER_MILLISECONDS = 300;
constexpr uint32_t ACCEPT_DEADLINE_MILLISECONDS = 30;
constexpr uint32_t NO_WAIT_MILLISECONDS = 0;
constexpr uint32_t MESSAGE_DELAY_MILLISECONDS = 40;
constexpr uint32_t LONG_WAIT_MILLISECONDS = 5000;
constexpr uint32_t OVERSIZED_TIMEOUT = 0x80000000;
constexpr uint32_t MESSAGE_VALUE = 0x4D53;
constexpr uint32_t WAKE_EVERYONE = 0xFFFF;
constexpr uint32_t NO_COUNT = 0;
constexpr uint32_t WORD_STEP_SECOND = 2;
constexpr uint32_t WORD_STEP_THIRD = 3;
constexpr uint32_t WORD_STEP_FOURTH = 4;
constexpr uint32_t ANSWER_OFFSET_SECOND = 2;
constexpr uint32_t ANSWER_OFFSET_THIRD = 3;
constexpr uint32_t ANSWER_OFFSET_FOURTH = 4;

alignas(PAGE_BYTES) uint8_t stacks[STACK_COUNT][STACK_BYTES];

uint32_t finished = 0;
bool callerOk[CALLER_COUNT] = {};
CallResult callerAnswer[CALLER_COUNT] = {};
uint32_t serverId = 0;
bool lateReplyAccepted = true;
uint32_t mainThread = 0;

void finish() {
	__atomic_add_fetch(&finished, 1, __ATOMIC_SEQ_CST);
	cock::user::futex_wake(&finished, WAKE_EVERYONE);
}

void waitFinished(uint32_t target) {
	for (;;) {
		uint32_t seen = __atomic_load_n(&finished, __ATOMIC_SEQ_CST);
		if (seen >= target) return;
		cock::user::futex_wait(&finished, seen);
	}
}

void echoServer(void *) {
	CallResult request = cock::user::ipc_accept();
	while (request.ok) {
		uint32_t answer[CALL_WORDS] = {request.words[0] + 1,
									   request.words[1] + ANSWER_OFFSET_SECOND,
									   request.words[2] + ANSWER_OFFSET_THIRD,
									   request.words[3] + ANSWER_OFFSET_FOURTH};
		if (request.words[0] == STOP_WORD) {
			cock::user::ipc_reply(answer);
			return;
		}
		request = cock::user::ipc_reply_wait(answer);
	}
}

void slowServer(void *) {
	cock::user::sleep_ms(QUEUE_MILLISECONDS);
	uint32_t sequence = 0;
	CallResult request = cock::user::ipc_accept();
	while (request.ok) {
		sequence++;
		uint32_t answer[CALL_WORDS] = {request.peer, request.words[0], sequence, 0};
		if (sequence == CALLER_COUNT) {
			cock::user::ipc_reply(answer);
			return;
		}
		request = cock::user::ipc_reply_wait(answer);
	}
}

void caller(void *argument) {
	size_t index = reinterpret_cast<size_t>(argument);
	uint32_t marker = index == FIRST_STACK ? FIRST_MARKER : SECOND_MARKER;
	uint32_t words[CALL_WORDS] = {marker, 0, 0, 0};
	callerAnswer[index] = cock::user::ipc_call(serverId, words);
	callerOk[index] = callerAnswer[index].ok;
	finish();
}

void forgetfulServer(void *) {
	CallResult request = cock::user::ipc_accept();
	if (request.ok) {
		cock::user::sleep_ms(SLOW_ANSWER_MILLISECONDS);
		const uint32_t answer[CALL_WORDS] = {request.words[0], 0, 0, 0};
		lateReplyAccepted = cock::user::ipc_reply(answer);
	}
	finish();
}

void messenger(void *) {
	cock::user::sleep_ms(MESSAGE_DELAY_MILLISECONDS);
	cock::ipc::Message message = {};
	message.type = cock::ipc::MessageType::SYS_PING;
	message.payload.args.arg0 = MESSAGE_VALUE;
	cock::user::ipc_send(mainThread, message);
	finish();
}

void quitter(void *) {
	cock::user::sleep_ms(QUEUE_MILLISECONDS);
	cock::user::ipc_accept();
}

void deferringServer(void *) {
	CallResult first = cock::user::ipc_accept();
	if (!first.ok) return;

	uint32_t ticket = cock::user::ipc_defer();
	CallResult second = cock::user::ipc_accept();
	if (!second.ok || ticket == cock::user::NO_TICKET) return;

	const uint32_t quick[CALL_WORDS] = {second.words[0], SECOND_SEQUENCE, 0, 0};
	cock::user::ipc_reply(quick);
	cock::user::sleep_ms(ORDER_MILLISECONDS);
	const uint32_t late[CALL_WORDS] = {first.words[0], FIRST_SEQUENCE, 0, 0};
	cock::user::ipc_reply_to(ticket, late);
}

} // namespace

bool synchronousCallsRoundTrip() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	uint32_t server = cock::user::thread_start(echoServer, nullptr, stacks[SERVER_STACK],
											   STACK_BYTES);
	if (server == NO_PROCESS) return false;

	for (uint32_t round = 0; round < ROUNDS; round++) {
		uint32_t words[CALL_WORDS] = {round, round * WORD_STEP_SECOND,
									  round * WORD_STEP_THIRD, round * WORD_STEP_FOURTH};
		CallResult answer = cock::user::ipc_call(server, words);
		if (!answer.ok || answer.peer != server ||
			answer.words[0] != words[0] + 1 ||
			answer.words[1] != words[1] + ANSWER_OFFSET_SECOND ||
			answer.words[2] != words[2] + ANSWER_OFFSET_THIRD ||
			answer.words[3] != words[3] + ANSWER_OFFSET_FOURTH)
			return false;
	}

	uint32_t stop[CALL_WORDS] = {STOP_WORD, 0, 0, 0};
	CallResult last = cock::user::ipc_call(server, stop);
	if (!last.ok || last.words[0] != STOP_WORD + 1) return false;

	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	return !cock::user::ipc_call(server, stop).ok;
}

bool queuedCallsAreServedInOrderAndMisuseIsRefused() {
	const uint32_t none[CALL_WORDS] = {};
	if (cock::user::ipc_call(UNKNOWN_THREAD, none).ok) return false;
	if (cock::user::ipc_call(cock::user::gettid(), none).ok) return false;
	if (cock::user::ipc_reply(none)) return false;

	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	serverId = cock::user::thread_start(slowServer, nullptr, stacks[SERVER_STACK],
										STACK_BYTES);
	if (serverId == NO_PROCESS) return false;

	uint32_t first = cock::user::thread_start(
		caller, reinterpret_cast<void *>(FIRST_STACK), stacks[FIRST_STACK], STACK_BYTES);
	cock::user::sleep_ms(ORDER_MILLISECONDS);
	uint32_t second = cock::user::thread_start(
		caller, reinterpret_cast<void *>(SECOND_STACK), stacks[SECOND_STACK], STACK_BYTES);
	if (first == NO_PROCESS || second == NO_PROCESS) return false;

	waitFinished(CALLER_COUNT);
	return callerOk[FIRST_STACK] && callerOk[SECOND_STACK] &&
		   callerAnswer[FIRST_STACK].words[0] == first &&
		   callerAnswer[FIRST_STACK].words[1] == FIRST_MARKER &&
		   callerAnswer[FIRST_STACK].words[2] == FIRST_SEQUENCE &&
		   callerAnswer[SECOND_STACK].words[0] == second &&
		   callerAnswer[SECOND_STACK].words[1] == SECOND_MARKER &&
		   callerAnswer[SECOND_STACK].words[2] == SECOND_SEQUENCE;
}

bool deadServerFailsEveryCaller() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	callerOk[FIRST_STACK] = true;
	callerOk[SECOND_STACK] = true;

	serverId = cock::user::thread_start(quitter, nullptr, stacks[SERVER_STACK],
										STACK_BYTES);
	if (serverId == NO_PROCESS) return false;
	if (cock::user::thread_start(caller, reinterpret_cast<void *>(FIRST_STACK),
								 stacks[FIRST_STACK], STACK_BYTES) == NO_PROCESS)
		return false;
	cock::user::sleep_ms(ORDER_MILLISECONDS);
	if (cock::user::thread_start(caller, reinterpret_cast<void *>(SECOND_STACK),
								 stacks[SECOND_STACK], STACK_BYTES) == NO_PROCESS)
		return false;

	waitFinished(CALLER_COUNT);
	return !callerOk[FIRST_STACK] && !callerOk[SECOND_STACK];
}

bool callTimeoutExpiresAndLateReplyFails() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	lateReplyAccepted = true;

	const uint32_t words[CALL_WORDS] = {FIRST_MARKER, 0, 0, 0};
	if (cock::user::ipc_call(serverId, words, OVERSIZED_TIMEOUT).status !=
		cock::user::CallStatus::FAILED)
		return false;

	uint32_t server = cock::user::thread_start(forgetfulServer, nullptr,
											   stacks[SERVER_STACK], STACK_BYTES);
	if (server == NO_PROCESS) return false;
	if (cock::user::ipc_call(server, words, NO_WAIT_MILLISECONDS).status !=
		cock::user::CallStatus::TIMED_OUT)
		return false;

	CallResult answer = cock::user::ipc_call(server, words, CALL_DEADLINE_MILLISECONDS);
	if (answer.ok || answer.status != cock::user::CallStatus::TIMED_OUT) return false;

	waitFinished(1);
	return !lateReplyAccepted;
}

bool acceptWaitsForMessagesAndTimesOut() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	mainThread = cock::user::gettid();

	if (cock::user::ipc_accept(ACCEPT_DEADLINE_MILLISECONDS).status !=
			cock::user::CallStatus::TIMED_OUT ||
		cock::user::ipc_accept(NO_WAIT_MILLISECONDS).status !=
			cock::user::CallStatus::TIMED_OUT)
		return false;

	if (cock::user::thread_start(messenger, nullptr, stacks[FIRST_STACK], STACK_BYTES) ==
		NO_PROCESS)
		return false;
	CallResult woken = cock::user::ipc_accept(LONG_WAIT_MILLISECONDS);
	if (woken.status != cock::user::CallStatus::MESSAGE_PENDING) return false;

	cock::ipc::Message received = {};
	bool delivered = cock::user::ipc_try_recv(received) == cock::user::Received::MESSAGE &&
					 received.payload.args.arg0 == MESSAGE_VALUE;
	waitFinished(1);
	return delivered;
}

bool deferredCallIsAnsweredAfterTheNextOne() {
	const uint32_t none[CALL_WORDS] = {};
	if (cock::user::ipc_defer() != cock::user::NO_TICKET) return false;
	if (cock::user::ipc_reply_to(UNKNOWN_THREAD, none)) return false;

	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	callerOk[FIRST_STACK] = false;
	callerOk[SECOND_STACK] = false;

	serverId = cock::user::thread_start(deferringServer, nullptr, stacks[SERVER_STACK],
										STACK_BYTES);
	if (serverId == NO_PROCESS) return false;
	if (cock::user::thread_start(caller, reinterpret_cast<void *>(FIRST_STACK),
								 stacks[FIRST_STACK], STACK_BYTES) == NO_PROCESS)
		return false;
	cock::user::sleep_ms(ORDER_MILLISECONDS);
	if (cock::user::thread_start(caller, reinterpret_cast<void *>(SECOND_STACK),
								 stacks[SECOND_STACK], STACK_BYTES) == NO_PROCESS)
		return false;

	waitFinished(CALLER_COUNT);
	return callerOk[FIRST_STACK] && callerOk[SECOND_STACK] &&
		   callerAnswer[FIRST_STACK].words[0] == FIRST_MARKER &&
		   callerAnswer[FIRST_STACK].words[1] == FIRST_SEQUENCE &&
		   callerAnswer[SECOND_STACK].words[0] == SECOND_MARKER &&
		   callerAnswer[SECOND_STACK].words[1] == SECOND_SEQUENCE;
}

} // namespace probe
