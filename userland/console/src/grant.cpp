#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <console/grant.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::console {

uint32_t waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) return message.payload.args.arg0;
	}
}

} // namespace cock::console
