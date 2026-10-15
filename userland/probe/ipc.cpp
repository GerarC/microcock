#include "probe.hpp"
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace probe {

namespace {

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

} // namespace probe
