#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <kbd/grant.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::kbd {

uint32_t waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED)
			return message.payload.args.arg0;
	}
}

} // namespace cock::kbd
