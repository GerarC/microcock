#include "probe.hpp"
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <stdint.h>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace probe {

namespace {

constexpr uint32_t UNKNOWN_PROCESS = 0x7FFFFFF0;
constexpr uint32_t SHORT_WAIT_MILLISECONDS = 30;
constexpr uint32_t LONG_WAIT_MILLISECONDS = 5000;
constexpr uint32_t NO_WAIT_MILLISECONDS = 0;
constexpr uintptr_t OVERSIZED_TIMEOUT = 0x80000000;

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

bool tryReceiveDoesNotBlock() {
	using cock::user::Received;
	uint32_t self = cock::user::self();

	Message received = {};
	if (cock::user::ipc_try_recv(received) != Received::EMPTY) return false;

	Message sent = {};
	sent.type = MessageType::SYS_PING;
	sent.payload.args.arg0 = PING_VALUE;
	if (!cock::user::ipc_send(self, sent)) return false;
	if (cock::user::ipc_try_recv(received) != Received::MESSAGE ||
		received.payload.args.arg0 != PING_VALUE)
		return false;
	if (cock::user::ipc_try_recv(received) != Received::EMPTY) return false;

	return syscall(SYS_IPC_TRY_RECV, KERNEL_ADDRESS, 0, 0, 0, 0, 0) ==
			   cock::user::FAILURE &&
		   syscall(SYS_IPC_TRY_RECV, UNMAPPED_USER_ADDRESS, 0, 0, 0, 0, 0) ==
			   cock::user::FAILURE;
}

bool receiveTimeoutBehaves() {
	using cock::user::Received;
	uint32_t self = cock::user::self();

	Message received = {};
	if (cock::user::ipc_recv_timeout(received, SHORT_WAIT_MILLISECONDS) !=
		Received::EMPTY)
		return false;
	if (cock::user::ipc_recv_timeout(received, NO_WAIT_MILLISECONDS) !=
		Received::EMPTY)
		return false;

	Message sent = {};
	sent.type = MessageType::SYS_PING;
	sent.payload.args.arg0 = PING_VALUE;
	if (!cock::user::ipc_send(self, sent)) return false;
	if (cock::user::ipc_recv_timeout(received, LONG_WAIT_MILLISECONDS) !=
			Received::MESSAGE ||
		received.payload.args.arg0 != PING_VALUE)
		return false;
	return cock::user::ipc_recv_timeout(received, SHORT_WAIT_MILLISECONDS) ==
		   Received::EMPTY;
}

bool timeoutCallsRefuseBadInput() {
	Message received = {};
	uintptr_t window = reinterpret_cast<uintptr_t>(&received);
	return syscall(SYS_IPC_RECV_TIMEOUT, KERNEL_ADDRESS, SHORT_WAIT_MILLISECONDS,
				   0, 0, 0, 0) == cock::user::FAILURE &&
		   syscall(SYS_IPC_RECV_TIMEOUT, UNMAPPED_USER_ADDRESS,
				   SHORT_WAIT_MILLISECONDS, 0, 0, 0, 0) == cock::user::FAILURE &&
		   syscall(SYS_IPC_RECV_TIMEOUT, window, OVERSIZED_TIMEOUT, 0, 0, 0, 0) ==
			   cock::user::FAILURE &&
		   syscall(SYS_SLEEP, OVERSIZED_TIMEOUT, 0, 0, 0, 0, 0) ==
			   cock::user::FAILURE;
}

bool sleepReturnsAndKeepsMessages() {
	using cock::user::Received;

	if (!cock::user::sleep_ms(NO_WAIT_MILLISECONDS)) return false;

	Message sent = {};
	sent.type = MessageType::SYS_PING;
	sent.payload.args.arg0 = PING_VALUE;
	if (!cock::user::ipc_send(cock::user::self(), sent)) return false;
	if (!cock::user::sleep_ms(SHORT_WAIT_MILLISECONDS)) return false;

	Message received = {};
	return cock::user::ipc_try_recv(received) == Received::MESSAGE &&
		   received.payload.args.arg0 == PING_VALUE;
}

bool processAliveReportsLiveness() {
	return cock::user::process_alive(cock::user::self()) &&
		   cock::user::process_alive(cock::user::names::server(cock::abi::fs::SERVER_NAME)) &&
		   !cock::user::process_alive(UNKNOWN_PROCESS) &&
		   !cock::user::process_alive(cock::user::NO_PROCESS);
}

} // namespace probe
