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

/** @brief Capability flag, `ENDPOINT` only: the holder chooses the badge of the handles it grants. */
constexpr uint32_t CAPABILITY_FLAG_MINT = 1u << 1;

/** @brief Capability flag, `ENDPOINT` only: the holder may bind a process to the endpoint with `SYS_EP_BIND`. Never passed on. */
constexpr uint32_t CAPABILITY_FLAG_BIND = 1u << 2;

/** @brief Value of the flags a query requires when it accepts any entry. */
constexpr uint32_t NO_REQUIRED_FLAGS = 0;

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
	uint32_t badge = 0; /**< `ENDPOINT` only: the value the kernel stamps on every message sent through this handle. */
	uint32_t handle = NO_CAPABILITY_NODE; /**< `ENDPOINT` only: the number a program passes to name this entry. `add` sets it to `node` when it is left as `NO_CAPABILITY_NODE`; a copy that `fork` makes keeps the number of the entry it came from, which the program may have stored. */

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
	 * @param capability Capability to add. When its `handle` is `NO_CAPABILITY_NODE`
	 * the stored entry gets its `node` as handle.
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
			if (entries[idx].handle == NO_CAPABILITY_NODE) entries[idx].handle = capability.node;
			return true;
		}
		return false;
	}

	/**
	 * @brief Calls a function for every entry that is in use.
	 * @param visit Callable taking a `const Capability &`; it must not change the set.
	 */
	template <typename Visitor>
	void forEach(Visitor visit) const {
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type != CapabilityType::NONE) visit(entries[idx]);
		}
	}

	/**
	 * @brief Tells whether a range is authorized.
	 * @param type Kind of resource.
	 * @param first First resource wanted.
	 * @param count Number of resources wanted.
	 * @return True when one entry of that type contains the whole range.
	 */
	bool covers(CapabilityType type, uint64_t first, uint64_t count) const {
		return find(type, first, count, NO_REQUIRED_FLAGS) != nullptr;
	}

	/**
	 * @brief Tells whether one entry contains a range and carries a flag.
	 * @param type Kind of resource.
	 * @param first First resource wanted.
	 * @param count Number of resources wanted.
	 * @param flag One `CAPABILITY_FLAG` value.
	 * @return True when one entry of that type contains the whole range and has the
	 * flag.
	 */
	bool hasFlag(CapabilityType type, uint64_t first, uint64_t count, uint32_t flag) const {
		return find(type, first, count, flag) != nullptr;
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
		return find(type, first, count, CAPABILITY_FLAG_GRANT);
	}

	/**
	 * @brief Tells whether the holder has grant authority over hardware or boot resources.
	 * @return True when at least one entry that is not of the `IPC` or `ENDPOINT`
	 * type has the grant flag. This is the test for operations reserved to the root
	 * server, such as labelling processes, finding boot modules, creating endpoints
	 * and sending capability messages. A holder that may only pass on `IPC` and
	 * `ENDPOINT` capabilities, as the name server does, has no such authority.
	 */
	bool holdsGrantAuthority() const {
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type != CapabilityType::NONE &&
				entries[idx].type != CapabilityType::IPC &&
				entries[idx].type != CapabilityType::ENDPOINT &&
				(entries[idx].flags & CAPABILITY_FLAG_GRANT))
				return true;
		}
		return false;
	}

	/**
	 * @brief Removes the entry that belongs to a node of the derivation tree.
	 * @param node Node id of the entry.
	 * @return True when an entry with that node existed. False for
	 * NO_CAPABILITY_NODE and for an unknown node.
	 */
	bool removeNode(CapabilityNodeId node) {
		if (node == NO_CAPABILITY_NODE) return false;
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type == CapabilityType::NONE || entries[idx].node != node)
				continue;
			entries[idx] = Capability();
			return true;
		}
		return false;
	}

	/**
	 * @brief Finds the endpoint handle that a number names.
	 * @param handle Number of the handle, as a program passes it: the node of the
	 * derivation tree for a capability the process was given, or the number it had in
	 * the parent for one that `fork` copied.
	 * @return The entry of type `ENDPOINT` whose `handle` is that number, or null for
	 * NO_CAPABILITY_NODE and for a number that names no endpoint capability of this
	 * set. The pointer is valid until the table changes.
	 */
	const Capability *endpointHandle(uint32_t handle) const {
		if (handle == NO_CAPABILITY_NODE) return nullptr;
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type == CapabilityType::ENDPOINT && entries[idx].handle == handle)
				return &entries[idx];
		}
		return nullptr;
	}

	/**
	 * @brief Tells whether any entry of a type satisfies a condition.
	 * @param type Kind of resource; the none type matches nothing.
	 * @param matches Callable taking a `const Capability &` and returning bool.
	 * @return True when `matches` accepts at least one entry of the type.
	 */
	template <typename Predicate>
	bool anyOf(CapabilityType type, Predicate matches) const {
		if (type == CapabilityType::NONE) return false;
		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			if (entries[idx].type == type && matches(entries[idx])) return true;
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
	 * @param required_flags Flags the entry must carry, `NO_REQUIRED_FLAGS` for any.
	 * @return The containing entry, or null for the none type, a zero count, a
	 * range that wraps around or no containing entry.
	 */
	const Capability *find(CapabilityType type, uint64_t first, uint64_t count,
						   uint32_t required_flags) const {
		if (type == CapabilityType::NONE || count == 0 ||
			first + count < first)
			return nullptr;

		for (size_t idx = 0; idx < MAX_CAPABILITIES; idx++) {
			const Capability &entry = entries[idx];
			if (entry.type != type) continue;
			if ((entry.flags & required_flags) != required_flags) continue;
			if (first >= entry.first && first + count <= entry.end())
				return &entry;
		}
		return nullptr;
	}

	Capability entries[MAX_CAPABILITIES]; /**< The table, empty slots have the none type. */
};

} // namespace cock::core::security

#endif // !CORE_SECURITY_CAPABILITY_HPP
