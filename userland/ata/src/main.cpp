#include <ata/devices.hpp>
#include <ata/grant.hpp>
#include <ata/pending.hpp>
#include <ata/requests.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::ipc::Message;

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" int main() {
	cock::ata::waitForCapabilities();

	if (!cock::ata::drive.init()) {
		printf("ata: no drive\n");
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}

	cock::ata::scanPartitions();
	printf("ata: %u sectors, %u devices\n", cock::ata::drive.sectors(), cock::ata::deviceCount);

	cock::ata::handlePending();

	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		cock::ata::handle(message);
	}
}
