#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <init/endpoints.hpp>
#include <init/grant.hpp>
#include <init/service.hpp>

using cock::abi::manifest::Entry;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::init {

void announce(uint32_t label, uint32_t argument) {
	Message message = {};
	message.type = MessageType::CAPABILITY_GRANTED;
	message.payload.args.arg0 = argument;
	sendTo(label, message);
}

void equip(const Entry &entry, uint32_t pid) {
	for (size_t index = 0; index < entry.grantCount; index++) {
		const cock::abi::manifest::Grant &grant = entry.grants[index];
		cock::user::cap_grant(pid, grant.type, grant.first, grant.count, grant.delegate);
	}

	if (entry.grantCount == 0 && !entry.hasArgument) return;
	announce(labelOf(entry), entry.argument);
}

} // namespace cock::init
