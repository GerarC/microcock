#ifndef COCK_USER_REPLY_HPP
#define COCK_USER_REPLY_HPP

#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

inline void yield_cpu() { syscall(SYS_SCHED_YIELD, 0, 0, 0, 0, 0, 0); }

template <typename Match>
bool receiveReply(uint32_t server, ipc::Message &answer, Match match,
				  void (*foreign)(const ipc::Message &)) {
	for (;;) {
		Received received = ipc_try_recv(answer);
		if (received == Received::FAILED) return false;
		if (received == Received::EMPTY) {
			if (!process_alive(server)) return false;
			yield_cpu();
			continue;
		}

		if (match(answer)) return true;
		if (foreign != nullptr) foreign(answer);
	}
}

} // namespace cock::user

#endif // !COCK_USER_REPLY_HPP
