#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <pipe/inbox.hpp>

namespace cock::pipe {

void discardMessages() {
	cock::ipc::Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE) {
	}
}

} // namespace cock::pipe
