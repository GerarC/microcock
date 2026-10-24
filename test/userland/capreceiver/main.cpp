#include <cock/abi/capability.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/capability_script.hpp>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::abi::CapabilityType;
using cock::abi::NO_CAPABILITY_NODE;
using cock::ipc::Message;
using cock::ipc::MessageType;
using namespace cock::user::capability_script;

namespace {

constexpr uint32_t INVALID_MASK = ~cock::abi::ALL_CAPABILITY_TYPES;
constexpr uint32_t NO_FLAGS = 0;
constexpr uint32_t TRANSFER_CHECK_COUNT = 2;

struct Transfers {
	uint32_t giver = 0;
	uint32_t arrived = 0;
	bool correct = true;
};

bool transferIsAsSent(const Message &message, uint32_t first, uint32_t flags) {
	return message.sender_pid != cock::ipc::KERNEL_SENDER &&
		   message.payload.args.arg0 == static_cast<uint32_t>(CapabilityType::IO_PORTS) &&
		   message.payload.args.arg1 == first && message.payload.args.arg2 == TRANSFER_PORT_COUNT &&
		   message.payload.args.arg3 == flags && message.payload.args.arg4 != NO_CAPABILITY_NODE &&
		   cock::user::request_ports(first, TRANSFER_PORT_COUNT);
}

void record(Transfers &transfers, const Message &message) {
	bool first = message.payload.args.arg5 == FIRST_TAG;
	bool valid = first ? transferIsAsSent(message, FIRST_TRANSFER_PORT, NO_FLAGS)
					   : message.payload.args.arg5 == SECOND_TAG &&
							 transferIsAsSent(message, SECOND_TRANSFER_PORT, cock::abi::GRANT_DELEGATE);
	transfers.correct = transfers.correct && valid;
	transfers.giver = message.sender_pid;
	transfers.arrived++;
}

bool answer(uint32_t giver, bool ok) {
	Message pong = {};
	pong.type = MessageType::SYS_PONG;
	pong.payload.args.arg0 = ok ? ANSWER_OK : ANSWER_WRONG;
	return cock::user::ipc_send(giver, pong);
}

bool copiesCannotBeStretched(uint32_t giver) {
	return !cock::user::request_ports(FIRST_TRANSFER_PORT, TRANSFER_PORT_COUNT * TRANSFER_CHECK_COUNT) &&
		   !cock::user::request_ports(UNTRANSFERRED_PORT, SINGLE_UNIT) &&
		   !cock::user::request_ports(UNHELD_PORT, SINGLE_UNIT) &&
		   !cock::user::cap_grant(giver, CapabilityType::IO_PORTS, FIRST_TRANSFER_PORT, SINGLE_UNIT) &&
		   cock::user::cap_send(giver, CapabilityType::IO_PORTS, FIRST_TRANSFER_PORT, SINGLE_UNIT) ==
			   NO_CAPABILITY_NODE;
}

bool delegatedCopyCanBePassedOn(uint32_t giver) {
	return cock::user::cap_grant(giver, CapabilityType::IO_PORTS, SECOND_TRANSFER_PORT, SINGLE_UNIT);
}

bool revocationTookOnlyTheFirst() {
	return !cock::user::request_ports(FIRST_TRANSFER_PORT, TRANSFER_PORT_COUNT) &&
		   cock::user::request_ports(SECOND_TRANSFER_PORT, TRANSFER_PORT_COUNT);
}

} // namespace

extern "C" int main() {
	bool passed = !cock::user::cap_accept(INVALID_MASK) &&
				  cock::user::cap_accept(cock::abi::capabilityBit(CapabilityType::IO_PORTS));
	Transfers transfers;
	Message message = {};
	for (;;) {
		if (!cock::user::ipc_recv(message)) break;

		if (message.type == MessageType::CAPABILITY_TRANSFER) {
			record(transfers, message);
			continue;
		}
		if (message.type != MessageType::SYS_PING) continue;

		if (message.payload.args.arg0 == STAGE_SENT) {
			bool ok = passed && transfers.correct && transfers.arrived == TRANSFER_CHECK_COUNT &&
					  copiesCannotBeStretched(transfers.giver) &&
					  delegatedCopyCanBePassedOn(transfers.giver);
			passed = ok;
			if (ok) puts("capreceiver: copies are as sent");
			answer(transfers.giver, ok);
			continue;
		}
		if (message.payload.args.arg0 == STAGE_REVOKED) {
			passed = passed && revocationTookOnlyTheFirst();
			puts(passed ? "capreceiver: ok" : "capreceiver: failed");
			answer(transfers.giver, passed);
			break;
		}
	}
	_exit(passed ? EXIT_PASSED : EXIT_FAILED);
}
