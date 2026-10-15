#ifndef CORE_SECURITY_CAPABILITY_HPP
#define CORE_SECURITY_CAPABILITY_HPP

#include <cock/abi/capability.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::security {

using cock::abi::CapabilityType;

constexpr uint32_t CAPABILITY_FLAG_GRANT = 1u << 0;

struct Capability {
	CapabilityType type = CapabilityType::NONE;
	uint32_t flags = 0;
	uint64_t first = 0;
	uint64_t count = 0;

	constexpr uint64_t end() const { return first + count; }
};

class CapabilitySet {
  public:
	static constexpr size_t MAX_CAPABILITIES = 16;

	bool add(const Capability &capability) {
		if (capability.type == CapabilityType::NONE || capability.count == 0 ||
			capability.first + capability.count < capability.first)
			return false;

		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type != CapabilityType::NONE) continue;
			entries[idx] = capability;
			return true;
		}
		return false;
	}

	bool covers(CapabilityType type, uint64_t first, uint64_t count) const {
		return find(type, first, count, false);
	}

	bool mayGrant(CapabilityType type, uint64_t first, uint64_t count) const {
		return find(type, first, count, true);
	}

	bool holdsGrantAuthority() const {
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type != CapabilityType::NONE &&
				(entries[idx].flags & CAPABILITY_FLAG_GRANT))
				return true;
		}
		return false;
	}

	void clear() {
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++)
			entries[idx] = Capability();
	}

  private:
	bool find(CapabilityType type, uint64_t first, uint64_t count,
			  bool needs_grant) const {
		if (type == CapabilityType::NONE || count == 0 ||
			first + count < first)
			return false;

		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			const Capability &entry = entries[idx];
			if (entry.type != type) continue;
			if (needs_grant && !(entry.flags & CAPABILITY_FLAG_GRANT)) continue;
			if (first >= entry.first && first + count <= entry.end())
				return true;
		}
		return false;
	}

	Capability entries[MAX_CAPABILITIES];
};

} // namespace cock::core::security

#endif // !CORE_SECURITY_CAPABILITY_HPP
