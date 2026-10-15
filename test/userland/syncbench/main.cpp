#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::CALL_WORDS;
using cock::user::CallResult;
using cock::user::NO_PROCESS;

namespace {

constexpr uint32_t ROUNDS = 2000;
constexpr size_t STACK_BYTES = 0x4000;
constexpr uint32_t STOP_VALUE = 0xFFFFFFFF;
constexpr uint64_t TSC_HIGH_SHIFT = 32;
constexpr uintptr_t EXIT_PASSED = 0;
constexpr uintptr_t EXIT_FAILED = 1;
constexpr uint32_t NO_VALUE = 0;

alignas(0x1000) uint8_t asyncStack[STACK_BYTES];
alignas(0x1000) uint8_t syncStack[STACK_BYTES];

uint64_t readCycles() {
	uint32_t low;
	uint32_t high;
	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return (static_cast<uint64_t>(high) << TSC_HIGH_SHIFT) | low;
}

void asyncServer(void *) {
	Message request = {};
	for (;;) {
		if (!cock::user::ipc_recv(request)) return;
		uint32_t value = request.payload.args.arg0;

		Message reply = {};
		reply.type = MessageType::SYS_PONG;
		reply.payload.args.arg0 = value + 1;
		cock::user::ipc_send(request.sender_pid, reply);
		if (value == STOP_VALUE) return;
	}
}

void syncServer(void *) {
	CallResult request = cock::user::ipc_accept();
	while (request.ok) {
		uint32_t answer[CALL_WORDS] = {request.words[0] + 1, NO_VALUE, NO_VALUE,
									   NO_VALUE};
		if (request.words[0] == STOP_VALUE) {
			cock::user::ipc_reply(answer);
			return;
		}
		request = cock::user::ipc_reply_wait(answer);
	}
}

bool measureAsync(uint64_t &cycles) {
	uint32_t server = cock::user::thread_start(asyncServer, nullptr, asyncStack, STACK_BYTES);
	if (server == NO_PROCESS) return false;

	Message request = {};
	request.type = MessageType::SYS_PING;
	Message reply = {};

	uint64_t start = readCycles();
	for (uint32_t round = 0; round < ROUNDS; round++) {
		request.payload.args.arg0 = round;
		if (!cock::user::ipc_send(server, request) || !cock::user::ipc_recv(reply) ||
			reply.payload.args.arg0 != round + 1)
			return false;
	}
	cycles = readCycles() - start;

	request.payload.args.arg0 = STOP_VALUE;
	cock::user::ipc_send(server, request);
	return cock::user::ipc_recv(reply);
}

bool measureSync(uint64_t &cycles) {
	uint32_t server = cock::user::thread_start(syncServer, nullptr, syncStack, STACK_BYTES);
	if (server == NO_PROCESS) return false;

	uint64_t start = readCycles();
	for (uint32_t round = 0; round < ROUNDS; round++) {
		uint32_t words[CALL_WORDS] = {round, NO_VALUE, NO_VALUE, NO_VALUE};
		CallResult answer = cock::user::ipc_call(server, words);
		if (!answer.ok || answer.words[0] != round + 1) return false;
	}
	cycles = readCycles() - start;

	uint32_t stop[CALL_WORDS] = {STOP_VALUE, NO_VALUE, NO_VALUE, NO_VALUE};
	return cock::user::ipc_call(server, stop).ok;
}

} // namespace

extern "C" void _start() {
	uint64_t asyncCycles = 0;
	uint64_t syncCycles = 0;
	bool measured = measureAsync(asyncCycles) && measureSync(syncCycles);
	if (!measured) {
		printf("syncbench: failed\n");
		cock::user::exit_process(EXIT_FAILED);
	}

	if (syncCycles >= asyncCycles) {
		printf("syncbench: sync is not faster\n");
		cock::user::exit_process(EXIT_FAILED);
	}

	printf("syncbench: ok async %u sync %u\n", static_cast<uint32_t>(asyncCycles) / ROUNDS,
		   static_cast<uint32_t>(syncCycles) / ROUNDS);
	cock::user::exit_process(EXIT_PASSED);
}
