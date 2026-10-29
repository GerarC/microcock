#include <cock/abi/mounts.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdio.h>
#include <vfs/mounts.hpp>
#include <vfs/pending.hpp>
#include <vfs/requests.hpp>

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" int main() {
	if (!cock::vfs::loadMounts()) {
		printf("vfs: cannot read %s\n", cock::abi::mounts::FILE_PATH);
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("vfs: %u mounts\n", static_cast<unsigned>(cock::vfs::mountCount));

	cock::ipc::Message message = {};
	for (;;) {
		if (!cock::vfs::takePending(message)) cock::user::ipc_recv(message);
		cock::vfs::handle(message);
	}
}
