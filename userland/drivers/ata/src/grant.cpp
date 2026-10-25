#include <ata/grant.hpp>
#include <ata/pending.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::ata {

void waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) return;
		keepPending(message);
	}
}

} // namespace cock::ata
