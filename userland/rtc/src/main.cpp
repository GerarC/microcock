#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <rtc/grant.hpp>
#include <rtc/pending.hpp>
#include <rtc/requests.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::ipc::Message;

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" void _start() {
	cock::rtc::waitForCapabilities();

	if (!cock::rtc::initClock()) {
		printf("rtc: no clock ports\n");
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("rtc: ready\n");

	cock::rtc::handlePending();

	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		cock::rtc::handle(message);
	}
}
