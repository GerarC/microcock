/** @file
 * @brief Capabilities held by a process: typed ranges that authorize
 * privileged operations, kept in a small fixed table with no allocation.
 */
#ifndef CORE_SECURITY_CAPABILITY_HPP
#define CORE_SECURITY_CAPABILITY_HPP

#include <cock/abi/capability.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::security {

using cock::abi::CapabilityNodeId;
using cock::abi::CapabilityType;
using cock::abi::NO_CAPABILITY_NODE;

/** @brief Capability flag: the holder may grant this capability, or part of
 * it, to other processes. */
constexpr uint32_t CAPABILITY_FLAG_GRANT = 1u << 0;

/**
 * @brief One authorization: a half open range [first, first + count) of
 * resources of a given type.
 *
 * The meaning of the range depends on the type: port numbers, interrupt lines,
 * physical addresses or boot module indexes. A default constructed value is
 * the empty slot.
 */
struct Capability {
	CapabilityType type = CapabilityType::NONE; /**< Kind of resource, none for an empty slot. */
	uint32_t flags = 0; /**< Combination of CAPABILITY_FLAG values. */
	CapabilityNodeId node = NO_CAPABILITY_NODE; /**< Node of the derivation tree that records where this entry came from. */
	uint64_t first = 0; /**< First resource of the range. */
	uint64_t count = 0; /**< Number of resources in the range. */

	/** @return First value past the range. Callers ensure it does not
	 * overflow, which add and find check. */
	constexpr uint64_t end() const { return first + count; }
};

/**
 * @brief The capabilities of one process, in a table of MAX_CAPABILITIES
 * entries.
 *
 * Queries require a range to be contained in a single entry, ranges of
 * different entries are not merged. A user supplied range with a zero length
 * or that wraps around never matches.
 */
class CapabilitySet {
  public:
	/** @brief Capacity of the table. */
	static constexpr size_t MAX_CAPABILITIES = 16;

	/**
	 * @brief Stores a capability in the first free slot.
	 * @param capability Capability to add.
	 * @return True when stored. False when the type is none, the count is zero,
	 * the range wraps around or the table is full.
	 */
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

	/**
	 * @brief Tells whether a range is authorized.
	 * @param type Kind of resource.
	 * @param first First resource wanted.
	 * @param count Number of resources wanted.
	 * @return True when one entry of that type contains the whole range.
	 */
	bool covers(CapabilityType type, uint64_t first, uint64_t count) const {
		return find(type, first, count, false) != nullptr;
	}

	/**
	 * @brief Tells whether a range may be given to another process.
	 * @param type Kind of resource.
	 * @param first First resource.
	 * @param count Number of resources.
	 * @return True when one entry that carries the grant flag contains the
	 * whole range.
	 */
	bool mayGrant(CapabilityType type, uint64_t first, uint64_t count) const {
		return grantable(type, first, count) != nullptr;
	}

	/**
	 * @brief Finds the entry that lets the holder give a range away.
	 * @param type Kind of resource.
	 * @param first First resource.
	 * @param count Number of resources.
	 * @return The first entry that carries the grant flag and contains the whole
	 * range, or null when there is none (including the none type, a zero count
	 * and a range that wraps around). The pointer is valid until the table
	 * changes.
	 */
	const Capability *grantable(CapabilityType type, uint64_t first,
								uint64_t count) const {
		return find(type, first, count, true);
	}

	/**
	 * @brief Tells whether the holder has any grantable capability at all.
	 * @return True when at least one entry has the grant flag. This is the
	 * test for operations reserved to the root server, such as spawning
	 * processes and sending capability messages.
	 */
	bool holdsGrantAuthority() const {
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type != CapabilityType::NONE &&
				(entries[idx].flags & CAPABILITY_FLAG_GRANT))
				return true;
		}
		return false;
	}

	/** @brief Removes every capability. */
	void clear() {
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++)
			entries[idx] = Capability();
	}

  private:
	/**
	 * @brief Searches the table for an entry that contains a range.
	 * @param type Kind of resource.
	 * @param first First resource.
	 * @param count Number of resources.
	 * @param needs_grant True to accept only entries with the grant flag.
	 * @return The containing entry, or null for the none type, a zero count, a
	 * range that wraps around or no containing entry.
	 */
	const Capability *find(CapabilityType type, uint64_t first, uint64_t count,
						   bool needs_grant) const {
		if (type == CapabilityType::NONE || count == 0 ||
			first + count < first)
			return nullptr;

		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			const Capability &entry = entries[idx];
			if (entry.type != type) continue;
			if (needs_grant && !(entry.flags & CAPABILITY_FLAG_GRANT)) continue;
			if (first >= entry.first && first + count <= entry.end())
				return &entry;
		}
		return nullptr;
	}

	Capability entries[MAX_CAPABILITIES]; /**< The table, empty slots have the none type. */
};

} // namespace cock::core::security

#endif // !CORE_SECURITY_CAPABILITY_HPP
