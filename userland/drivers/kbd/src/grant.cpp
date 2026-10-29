#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <kbd/grant.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::kbd {

void waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) return;
	}
}

} // namespace cock::kbd
