#include <cock/abi/service.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <pipe/ends.hpp>
#include <pipe/inbox.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::pipe {

void takeMessages() {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE) {
		if (message.type != MessageType::PM_FORKED || message.badge != cock::abi::service::ROOT_PID) continue;
		copyEnds(message.payload.args.arg1, message.payload.args.arg0);
	}
}

} // namespace cock::pipe
