#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <ext/grant.hpp>
#include <ext/server.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::ext {

uint32_t waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) return message.payload.args.arg0;
		Server::defer(message);
	}
}

} // namespace cock::ext
