#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <ramfs/archive.hpp>
#include <ramfs/pending.hpp>
#include <ramfs/requests.hpp>
#include <stdint.h>

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
		cock::ramfs::keepPending(message);
	}

	if (!cock::ramfs::loadArchive()) cock::user::exit_process(EXIT_FAILED_STATUS);

	cock::ramfs::handlePending();

	for (;;) {
		cock::user::ipc_recv(message);
		cock::ramfs::handle(message);
	}
}
