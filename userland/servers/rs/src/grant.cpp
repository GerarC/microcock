#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <rs/endpoints.hpp>
#include <rs/grant.hpp>
#include <rs/service.hpp>

using cock::abi::manifest::Entry;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::rs {

void announce(uint32_t label, uint32_t argument, uint32_t second) {
	Message message = {};
	message.type = MessageType::CAPABILITY_GRANTED;
	message.payload.args.arg0 = argument;
	message.payload.args.arg1 = second;
	sendTo(label, message);
}

void grantCapabilities(const Entry &entry, uint32_t pid) {
	for (size_t index = 0; index < entry.grantCount; index++) {
		const cock::abi::manifest::Grant &grant = entry.grants[index];
		cock::user::cap_grant(pid, grant.type, grant.first, grant.count, grant.delegate);
	}
}

void equip(const Entry &entry, uint32_t pid) {
	grantCapabilities(entry, pid);

	if (entry.grantCount == 0 && !entry.hasArgument) return;
	announce(labelOf(entry), entry.argument);
}

} // namespace cock::rs
