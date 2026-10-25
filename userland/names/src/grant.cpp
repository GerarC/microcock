#include <cock/abi/service.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <names/grant.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::names {

void waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED &&
			message.badge == cock::abi::service::ROOT_PID)
			return;
	}
}

} // namespace cock::names
