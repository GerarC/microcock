#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <init/grant.hpp>

using cock::abi::manifest::Entry;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace cock::init {

void announce(uint32_t pid, uint32_t peer, uint32_t argument) {
	Message message = {};
	message.type = MessageType::CAPABILITY_GRANTED;
	message.payload.args.arg0 = peer;
	message.payload.args.arg1 = argument;
	cock::user::ipc_send(pid, message);
}

void equip(const Entry &entry, uint32_t pid) {
	for (size_t index = 0; index < entry.grantCount; index++) {
		const cock::abi::manifest::Grant &grant = entry.grants[index];
		cock::user::cap_grant(pid, grant.type, grant.first, grant.count);
	}

	if (entry.grantCount == 0 && !entry.hasPeer() && !entry.hasArgument) return;
	uint32_t peer = entry.hasPeer() ? cock::user::lookup(entry.peer) : NO_PROCESS;
	announce(pid, peer, entry.argument);
}

} // namespace cock::init
