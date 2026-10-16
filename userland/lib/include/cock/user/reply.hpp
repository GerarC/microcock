/**
 * @file reply.hpp
 * @brief Helper that waits for the reply of an asynchronous request to a server.
 */
#ifndef COCK_USER_REPLY_HPP
#define COCK_USER_REPLY_HPP

#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

/** Interval, in milliseconds, at which a waiting client checks that the server is alive. */
constexpr uint32_t SERVER_POLL_MILLISECONDS = 20;

/**
 * @brief Waits for the message that answers a request, handing other messages to a callback.
 *
 * Blocks in `ipc_recv_timeout` slices of `SERVER_POLL_MILLISECONDS`. Each time a slice
 * ends with no message it checks `process_alive` for the server, so a dead server ends
 * the wait. It does not look at the stash and has no overall time limit while the server
 * lives.
 * @tparam Match Callable taking `const ipc::Message &` and returning bool.
 * @param server Process id of the server that must answer.
 * @param answer Receives each message; holds the matching one on success.
 * @param match Predicate that accepts the awaited reply (type, sender and request id).
 * @param foreign Called with each message that does not match; null drops them.
 * @return true when a matching message arrived; false when the receive failed or the
 *         server is no longer alive.
 */
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
