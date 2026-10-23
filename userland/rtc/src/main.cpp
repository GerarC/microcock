#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <rtc/pending.hpp>
#include <rtc/requests.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" void _start() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) break;
		cock::rtc::keepPending(message);
	}

	if (!cock::rtc::initClock()) {
		printf("rtc: no clock ports\n");
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("rtc: ready\n");

	cock::rtc::handlePending();

	for (;;) {
		cock::user::ipc_recv(message);
		cock::rtc::handle(message);
	}
}
