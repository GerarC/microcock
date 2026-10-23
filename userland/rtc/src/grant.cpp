#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <rtc/grant.hpp>
#include <rtc/pending.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::rtc {

void waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) return;
		keepPending(message);
	}
}

} // namespace cock::rtc
