#ifndef COCK_USER_REPLY_HPP
#define COCK_USER_REPLY_HPP

#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

constexpr uint32_t SERVER_POLL_MILLISECONDS = 20;

template <typename Match>
bool receiveReply(uint32_t server, ipc::Message &answer, Match match,
				  void (*foreign)(const ipc::Message &)) {
	for (;;) {
		Received received = ipc_recv_timeout(answer, SERVER_POLL_MILLISECONDS);
		if (received == Received::FAILED) return false;
		if (received == Received::EMPTY) {
			if (!process_alive(server)) return false;
			continue;
		}

		if (match(answer)) return true;
		if (foreign != nullptr) foreign(answer);
	}
}

} // namespace cock::user

#endif // !COCK_USER_REPLY_HPP
