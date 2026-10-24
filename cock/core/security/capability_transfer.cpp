#include <cock/core/security/capability_transfer.hpp>
#include <cock/core/security/capability_tree.hpp>

namespace cock::core::security {

namespace {

constexpr uint32_t NO_FLAGS = 0;

} // namespace

CapabilityNodeId delegate_capability(const task::Process &giver, task::Process &receiver,
									 CapabilityType type, uint64_t first, uint64_t count,
									 bool delegate) {
	const Capability *source = giver.getCapabilities().grantable(type, first, count);
	if (source == nullptr) return NO_CAPABILITY_NODE;

	CapabilityNodeId node =
		capability_tree.derive(source->node, receiver.getId(), type, first, count);
	if (node == NO_CAPABILITY_NODE) return NO_CAPABILITY_NODE;

	Capability copy;
	copy.type = type;
	copy.flags = delegate ? CAPABILITY_FLAG_GRANT : NO_FLAGS;
	copy.first = first;
	copy.count = count;
	copy.node = node;
	if (!receiver.getCapabilities().add(copy)) {
		capability_tree.detach(node);
		return NO_CAPABILITY_NODE;
	}
	return node;
}

bool withdraw_capability(task::Process &receiver, CapabilityNodeId node) {
	bool removed = receiver.getCapabilities().removeNode(node);
	bool detached = capability_tree.detach(node);
	return removed && detached;
}

} // namespace cock::core::security
