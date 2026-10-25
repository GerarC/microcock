#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <rs/pm.hpp>
#include <rs/supervise.hpp>
#include <stddef.h>

using cock::ipc::Message;

namespace {

constexpr uint32_t SUPERVISE_POLL_MILLISECONDS = 20;

} // namespace

namespace cock::rs {

void keepExitNotice(const Message &message) {
	if (isSpawnNotice(message))
		startedForInit(message.payload.args.arg0, message.payload.args.arg1);
	else if (isExitNotice(message))
		cock::user::stash_message(message);
}

bool awaitExit(uint32_t pid) {
	Message message = {};
	size_t kept = cock::user::stashedCount;
	bool noticed = false;
	for (size_t index = 0; index < kept; index++) {
		cock::user::take_stashed(message);
		if (!noticed && isExitNotice(message) && message.payload.args.arg0 == pid)
			noticed = true;
		else
			cock::user::stash_message(message);
	}
	if (noticed) return true;

	for (;;) {
		cock::user::Received received =
			cock::user::ipc_recv_timeout(message, SUPERVISE_POLL_MILLISECONDS);
		if (received == cock::user::Received::MESSAGE) {
			if (isExitNotice(message) && message.payload.args.arg0 == pid) return true;
			keepExitNotice(message);
			continue;
		}
		if (received == cock::user::Received::FAILED || !cock::user::process_alive(pid))
			return false;
	}
}

} // namespace cock::rs
