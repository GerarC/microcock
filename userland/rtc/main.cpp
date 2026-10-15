#include <cock/abi/datetime.hpp>
#include <cock/abi/rtc.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/cmos.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::rtc::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr size_t MAX_PENDING = 16;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;

cock::user::Cmos cmos;
Message pending[MAX_PENDING];
size_t pendingCount = 0;

void reply(uint32_t pid, uint32_t id, Status status, uint32_t date = 0, uint32_t time = 0) {
	Message answer = {};
	answer.type = MessageType::RTC_REPLY;
	answer.payload.args.arg0 = id;
	answer.payload.args.arg1 = static_cast<uint32_t>(status);
	answer.payload.args.arg2 = date;
	answer.payload.args.arg3 = time;
	cock::user::ipc_send(pid, answer);
}

void handle(const Message &message) {
	if (message.sender_pid == cock::ipc::KERNEL_SENDER) return;
	if (message.type != MessageType::RTC_READ) return;

	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	cock::abi::datetime::DateTime now = {};
	if (!cmos.now(now)) return reply(pid, id, Status::UNAVAILABLE);
	reply(pid, id, Status::OK, cock::abi::rtc::packDate(now), cock::abi::rtc::packTime(now));
}

} // namespace

extern "C" void _start() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) break;
		if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
	}

	if (!cmos.init()) {
		printf("rtc: no clock ports\n");
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("rtc: ready\n");

	for (size_t index = 0; index < pendingCount; index++)
		handle(pending[index]);

	for (;;) {
		cock::user::ipc_recv(message);
		handle(message);
	}
}
