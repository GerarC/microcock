#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <fat/grant.hpp>
#include <fat/server.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::fat {

uint32_t waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) return message.payload.args.arg1;
		Server::defer(message);
	}
}

} // namespace cock::fat
