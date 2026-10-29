#include <cock/ipc/message.hpp>
#include <rs/endpoints.hpp>
#include <rs/forks.hpp>
#include <rs/manifest.hpp>
#include <rs/pm.hpp>
#include <rs/service.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::rs {

void forwardFork(uint32_t child, uint32_t parent) {
	Message notice = {};
	notice.type = MessageType::PM_FORKED;
	notice.payload.args.arg0 = child;
	notice.payload.args.arg1 = parent;

	for (size_t index = 0; index < manifest.count; index++) {
		const cock::abi::manifest::Entry &entry = manifest.entries[index];
		if (entry.notifiedOfForks) sendTo(labelOf(entry), notice);
	}
}

void acknowledgeFork(uint32_t child) {
	const cock::abi::manifest::Entry *pm = pmEntry();
	if (pm == nullptr) return;

	Message settled = {};
	settled.type = MessageType::PM_FORK_SETTLED;
	settled.payload.args.arg0 = child;
	sendTo(labelOf(*pm), settled);
}

} // namespace cock::rs
