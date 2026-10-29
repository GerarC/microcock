#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/capability_script.hpp>
#include <cock/user/names.hpp>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::abi::CapabilityType;
using cock::abi::NO_CAPABILITY_NODE;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;
using namespace cock::user::capability_script;

namespace {

constexpr uint32_t ATTEMPT_MILLISECONDS = 50;
constexpr uint32_t EMPTY_COUNT = 0;
constexpr uint32_t UNKNOWN_TYPE_VALUE =
	static_cast<uint32_t>(cock::abi::HIGHEST_CAPABILITY_TYPE) + 1;

bool waitForGrant() {
	Message message = {};
	do {
		if (!cock::user::ipc_recv(message)) return false;
	} while (message.type != MessageType::CAPABILITY_GRANTED);
	return true;
}

cock::user::names::Connection findReceiver() {
	cock::user::names::Connection connection = {};
	for (uint32_t attempt = 0; attempt < WAIT_ROUNDS; attempt++) {
		connection = cock::user::names::connect(RECEIVER_NAME);
		if (connection.pid != NO_PROCESS && connection.handle != NO_CAPABILITY_NODE)
			return connection;
		cock::user::sleep_ms(ATTEMPT_MILLISECONDS);
	}
	connection.pid = NO_PROCESS;
	return connection;
}

uint32_t sendFirstTransfer(uint32_t receiver) {
	for (uint32_t attempt = 0; attempt < WAIT_ROUNDS; attempt++) {
		uint32_t node = cock::user::cap_send(receiver, CapabilityType::IO_PORTS, FIRST_TRANSFER_PORT,
											 TRANSFER_PORT_COUNT, false, FIRST_TAG);
		if (node != NO_CAPABILITY_NODE) return node;
		cock::user::sleep_ms(ATTEMPT_MILLISECONDS);
	}
	return NO_CAPABILITY_NODE;
}

bool refusesWhatItMust(uint32_t receiver, uint32_t receiverProcess) {
	CapabilityType unknown = static_cast<CapabilityType>(UNKNOWN_TYPE_VALUE);
	return cock::user::cap_send(receiver, CapabilityType::IRQ, GIVER_IRQ_LINE, SINGLE_UNIT) ==
			   NO_CAPABILITY_NODE &&
		   cock::user::cap_send(receiver, CapabilityType::IO_PORTS, UNHELD_PORT, SINGLE_UNIT) ==
			   NO_CAPABILITY_NODE &&
		   cock::user::cap_send(receiver, CapabilityType::IO_PORTS, FIRST_TRANSFER_PORT, EMPTY_COUNT) ==
			   NO_CAPABILITY_NODE &&
		   cock::user::cap_send(receiver, CapabilityType::NONE, FIRST_TRANSFER_PORT, SINGLE_UNIT) ==
			   NO_CAPABILITY_NODE &&
		   cock::user::cap_send(receiver, unknown, FIRST_TRANSFER_PORT, SINGLE_UNIT) ==
			   NO_CAPABILITY_NODE &&
		   cock::user::cap_send_local(cock::user::self(), CapabilityType::IO_PORTS, FIRST_TRANSFER_PORT,
									  SINGLE_UNIT) == NO_CAPABILITY_NODE &&
		   cock::user::cap_send_local(receiverProcess, CapabilityType::IO_PORTS, FIRST_TRANSFER_PORT,
									  SINGLE_UNIT) == NO_CAPABILITY_NODE;
}

bool tell(uint32_t receiver, uint32_t stage) {
	Message ping = {};
	ping.type = MessageType::SYS_PING;
	ping.payload.args.arg0 = stage;
	return cock::user::ep_send(receiver, ping, true);
}

bool waitForAnswer() {
	Message answer = {};
	for (uint32_t round = 0; round < WAIT_ROUNDS; round++) {
		cock::user::Received received =
			cock::user::ipc_recv_timeout(answer, ANSWER_WAIT_MILLISECONDS);
		if (received == cock::user::Received::FAILED) return false;
		if (received == cock::user::Received::EMPTY) continue;
		if (answer.type == MessageType::SYS_PONG && answer.badge == cock::abi::endpoint::NO_BADGE)
			return answer.payload.args.arg0 == ANSWER_OK;
	}
	return false;
}

} // namespace

extern "C" int main() {
	bool passed = waitForGrant();
	cock::user::names::Connection connection = {};
	if (passed) connection = findReceiver();
	uint32_t receiver = connection.handle;
	passed = passed && connection.pid != NO_PROCESS && receiver != NO_CAPABILITY_NODE;

	uint32_t first = passed ? sendFirstTransfer(receiver) : NO_CAPABILITY_NODE;
	passed = passed && first != NO_CAPABILITY_NODE;
	if (passed) puts("capgiver: first transfer sent");

	passed = passed && refusesWhatItMust(receiver, connection.pid);
	if (passed) puts("capgiver: refusals hold");

	passed = passed && cock::user::cap_send(receiver, CapabilityType::IO_PORTS, SECOND_TRANSFER_PORT,
											TRANSFER_PORT_COUNT, true, SECOND_TAG) != NO_CAPABILITY_NODE;
	passed = passed && tell(receiver, STAGE_SENT) && waitForAnswer();
	if (passed) puts("capgiver: receiver checked the copies");

	passed = passed && cock::user::cap_revoke(first) && tell(receiver, STAGE_REVOKED) &&
			 waitForAnswer();

	puts(passed ? "capgiver: ok" : "capgiver: failed");
	_exit(passed ? EXIT_PASSED : EXIT_FAILED);
}
