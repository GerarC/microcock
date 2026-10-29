#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <ramfs/archive.hpp>
#include <ramfs/grant.hpp>
#include <ramfs/pending.hpp>
#include <ramfs/requests.hpp>
#include <stdint.h>

using cock::ipc::Message;

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" int main() {
	cock::ramfs::waitForCapabilities();

	if (!cock::ramfs::loadArchive()) cock::user::exit_process(EXIT_FAILED_STATUS);

	cock::ramfs::handlePending();

	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		cock::ramfs::handle(message);
	}
}
