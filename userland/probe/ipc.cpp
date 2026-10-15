#include "probe.hpp"
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace probe {

namespace {

constexpr uint32_t UNKNOWN_PROCESS = 0x7FFFFFF0;

bool refusesReservedMessage(MessageType type) {
	Message message = {};
	message.type = type;
	return !cock::user::ipc_send(cock::user::self(), message);
}

} // namespace

bool irqNotificationIsRefused() {
	return refusesReservedMessage(MessageType::IRQ_NOTIFICATION);
}

bool capabilityGrantIsRefused() {
	return refusesReservedMessage(MessageType::CAPABILITY_GRANTED);
}

bool messageToSelfArrives() {
	uint32_t self = cock::user::self();

	Message sent = {};
	sent.type = MessageType::SYS_PING;
	sent.payload.args.arg0 = PING_VALUE;
	if (!cock::user::ipc_send(self, sent)) return false;

	Message received = {};
	if (!cock::user::ipc_recv(received)) return false;
	return received.type == MessageType::SYS_PING &&
		   received.sender_pid == self &&
		   received.payload.args.arg0 == PING_VALUE;
}

bool processAliveReportsLiveness() {
	return cock::user::process_alive(cock::user::self()) &&
		   cock::user::process_alive(cock::user::lookup(cock::abi::fs::SERVER_NAME)) &&
		   !cock::user::process_alive(UNKNOWN_PROCESS) &&
		   !cock::user::process_alive(cock::user::NO_PROCESS);
}

} // namespace probe
