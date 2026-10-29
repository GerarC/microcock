#include <cock/core/security/capability_inherit.hpp>
#include <cock/core/security/capability_tree.hpp>

namespace cock::core::security {

namespace {

constexpr uint32_t NO_FLAGS = 0;

bool names_the_parent(const task::Process &parent, const Capability &capability) {
	return capability.type == CapabilityType::ENDPOINT && capability.badge == parent.getId();
}

} // namespace

bool is_inheritable(const Capability &capability) {
	if (capability.type == CapabilityType::IPC) return true;
	return capability.type == CapabilityType::ENDPOINT && (capability.flags & CAPABILITY_FLAG_BIND) == 0;
}

bool can_inherit_capabilities(const task::Process &parent) {
	size_t count = 0;
	bool linked = true;
	parent.getCapabilities().forEach([&](const Capability &capability) {
		if (!is_inheritable(capability)) return;
		count++;
		if (capability_tree.find(capability.node) == nullptr) linked = false;
	});
	return linked && capability_tree.hasRoom(count);
}

bool inherit_capabilities(const task::Process &parent, task::Process &child) {
	bool copied = true;
	parent.getCapabilities().forEach([&](const Capability &source) {
		if (!copied || !is_inheritable(source)) return;

		Capability copy;
		copy.type = source.type;
		copy.flags = NO_FLAGS;
		copy.badge = names_the_parent(parent, source) ? child.getId() : source.badge;
		copy.first = source.first;
		copy.count = source.count;
		copy.handle = source.handle;
		copy.node = capability_tree.derive(source.node, child.getId(), source.type, source.first,
										   source.count);
		if (copy.node == NO_CAPABILITY_NODE) {
			copied = false;
			return;
		}
		if (!child.getCapabilities().add(copy)) {
			capability_tree.detach(copy.node);
			copied = false;
		}
	});

	if (!copied) {
		child.getCapabilities().clear();
		capability_tree.releaseHolder(child.getId());
	}
	return copied;
}

} // namespace cock::core::security
