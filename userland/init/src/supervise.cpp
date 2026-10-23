#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <init/supervise.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr uint32_t SUPERVISE_POLL_MILLISECONDS = 20;

} // namespace

namespace cock::init {

bool awaitExit(uint32_t pid) {
	Message message = {};
	for (;;) {
		cock::user::Received received =
			cock::user::ipc_recv_timeout(message, SUPERVISE_POLL_MILLISECONDS);
		if (received == cock::user::Received::MESSAGE) {
			if (message.type == MessageType::PROCESS_EXITED &&
				message.sender_pid == cock::ipc::KERNEL_SENDER &&
				message.payload.args.arg0 == pid)
				return true;
			continue;
		}
		if (received == cock::user::Received::FAILED || !cock::user::process_alive(pid))
			return false;
	}
}

} // namespace cock::init
