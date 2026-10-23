/** @file
 * @brief The derivation tree of capabilities: which capability was given out of
 * which, and who holds each one. A fixed table, no allocation.
 */
#ifndef CORE_SECURITY_CAPABILITY_TREE_HPP
#define CORE_SECURITY_CAPABILITY_TREE_HPP

#include <cock/core/security/capability.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::security {

/** @brief Highest node id handed out. Ids stay below the value `-1` that the
 * system calls use for failure, so an id can be returned as a result. */
constexpr CapabilityNodeId HIGHEST_CAPABILITY_NODE = 0x7FFFFFFF;

/** @brief First node id handed out, and the one after the highest wraps to. */
constexpr CapabilityNodeId FIRST_CAPABILITY_NODE = 1;

/**
 * @brief One record of the tree: a capability held by one process and the
 * node it was derived from.
 *
 * A free slot has the id `NO_CAPABILITY_NODE`. A root node (a capability the
 * kernel gave to the first program) has the parent `NO_CAPABILITY_NODE`.
 */
struct CapabilityNode {
	CapabilityNodeId id = NO_CAPABILITY_NODE; /**< Identifier, never reused while the node exists. */
	CapabilityNodeId parent = NO_CAPABILITY_NODE; /**< Node this one was derived from, none for a root. */
	uint32_t holder = 0; /**< Id of the process that holds the capability. */
	CapabilityType type = CapabilityType::NONE; /**< Kind of resource. */
	uint64_t first = 0; /**< First resource of the range. */
	uint64_t count = 0; /**< Number of resources in the range. */

	/** @return First value past the range. */
	constexpr uint64_t end() const { return first + count; }
};

/**
 * @brief The table of all derived capabilities of the system.
 *
 * Every `SYS_CAP_GRANT` adds a node whose range lies inside the range of its
 * parent, so the tree records the delegation chain. When a holder dies its
 * nodes are removed and their children move up to the grandparent, so the
 * chain stays connected to a root. The table has MAX_NODES entries; when it is
 * full a grant fails, like when the table of a process is full.
 *
 * Callers run with interrupts off (system calls) or hold an interrupt guard.
 */
class CapabilityTree {
  public:
	/** @brief Capacity of the table. */
	static constexpr size_t MAX_NODES = 256;

	/**
	 * @brief Records a capability that comes from no other, such as the ones
	 * the kernel gives to the first program.
	 * @param holder Id of the process that holds it.
	 * @param type Kind of resource.
	 * @param first First resource of the range.
	 * @param count Number of resources.
	 * @return The new node id, or NO_CAPABILITY_NODE when the type is none, the
	 * count is zero, the range wraps around or the table is full.
	 */
	CapabilityNodeId createRoot(uint32_t holder, CapabilityType type,
								uint64_t first, uint64_t count) {
		if (!validRange(type, first, count)) return NO_CAPABILITY_NODE;
		return insert(NO_CAPABILITY_NODE, holder, type, first, count);
	}

	/**
	 * @brief Records a capability given out of another one.
	 * @param parent Node the new one is derived from; it must exist.
	 * @param holder Id of the process that receives it.
	 * @param type Kind of resource; it must be the type of the parent.
	 * @param first First resource of the range.
	 * @param count Number of resources.
	 * @return The new node id, or NO_CAPABILITY_NODE when the parent does not
	 * exist, the type differs, the range is not inside the range of the parent,
	 * the range is invalid or the table is full.
	 */
	CapabilityNodeId derive(CapabilityNodeId parent, uint32_t holder,
							CapabilityType type, uint64_t first,
							uint64_t count) {
		if (!validRange(type, first, count)) return NO_CAPABILITY_NODE;
		const CapabilityNode *origin = find(parent);
		if (origin == nullptr || origin->type != type) return NO_CAPABILITY_NODE;
		if (first < origin->first || first + count > origin->end())
			return NO_CAPABILITY_NODE;
		return insert(parent, holder, type, first, count);
	}

	/**
	 * @brief Looks a node up.
	 * @param id Node id.
	 * @return The node, or null for NO_CAPABILITY_NODE and for an id that is not
	 * in the table. The pointer is valid until the table changes.
	 */
	const CapabilityNode *find(CapabilityNodeId id) const {
		if (id == NO_CAPABILITY_NODE) return nullptr;
		for (size_t idx = 0; idx < MAX_NODES; idx++) {
			if (nodes[idx].id == id) return &nodes[idx];
		}
		return nullptr;
	}

	/**
	 * @brief Removes one node. Its children are attached to its parent.
	 * @param id Node to remove.
	 * @return True when the node existed.
	 */
	bool detach(CapabilityNodeId id) {
		CapabilityNode *node = locate(id);
		if (node == nullptr) return false;

		for (size_t idx = 0; idx < MAX_NODES; idx++) {
			if (nodes[idx].id != NO_CAPABILITY_NODE && nodes[idx].parent == id)
				nodes[idx].parent = node->parent;
		}
		*node = CapabilityNode();
		used--;
		return true;
	}

	/**
	 * @brief Removes every node a process holds. Used when the process ends.
	 * @param holder Id of the process.
	 * @return Number of nodes removed.
	 */
	size_t releaseHolder(uint32_t holder) {
		size_t removed = 0;
		for (size_t idx = 0; idx < MAX_NODES; idx++) {
			if (nodes[idx].id != NO_CAPABILITY_NODE && nodes[idx].holder == holder &&
				detach(nodes[idx].id))
				removed++;
		}
		return removed;
	}

	/** @return Number of nodes in the table. */
	size_t size() const { return used; }

	/** @brief Empties the table and restarts the ids. Used by tests. */
	void reset() {
		for (size_t idx = 0; idx < MAX_NODES; idx++)
			nodes[idx] = CapabilityNode();
		used = 0;
		nextId = FIRST_CAPABILITY_NODE;
	}

  private:
	/**
	 * @brief Checks a type and a range.
	 * @return False for the none type, a zero count or a range that wraps around.
	 */
	static bool validRange(CapabilityType type, uint64_t first, uint64_t count) {
		return type != CapabilityType::NONE && count != 0 && first + count >= first;
	}

	/** @return The node with an id, writable, or null. */
	CapabilityNode *locate(CapabilityNodeId id) {
		return const_cast<CapabilityNode *>(find(id));
	}

	/** @return The next id that no node uses. Callers know the table is not full. */
	CapabilityNodeId takeId() {
		CapabilityNodeId candidate;
		do {
			candidate = nextId;
			nextId = nextId == HIGHEST_CAPABILITY_NODE ? FIRST_CAPABILITY_NODE
													  : nextId + 1;
		} while (find(candidate) != nullptr);
		return candidate;
	}

	/** @return The new node id, or NO_CAPABILITY_NODE when the table is full. */
	CapabilityNodeId insert(CapabilityNodeId parent, uint32_t holder,
							CapabilityType type, uint64_t first, uint64_t count) {
		if (used == MAX_NODES) return NO_CAPABILITY_NODE;
		for (size_t idx = 0; idx < MAX_NODES; idx++) {
			if (nodes[idx].id != NO_CAPABILITY_NODE) continue;
			nodes[idx].id = takeId();
			nodes[idx].parent = parent;
			nodes[idx].holder = holder;
			nodes[idx].type = type;
			nodes[idx].first = first;
			nodes[idx].count = count;
			used++;
			return nodes[idx].id;
		}
		return NO_CAPABILITY_NODE;
	}

	CapabilityNode nodes[MAX_NODES]; /**< The table, free slots have the id none. */
	size_t used = 0; /**< Number of occupied slots. */
	CapabilityNodeId nextId = FIRST_CAPABILITY_NODE; /**< Candidate for the next id. */
};

/** @brief The derivation tree of the running system. */
inline constinit CapabilityTree capability_tree;

} // namespace cock::core::security

#endif // !CORE_SECURITY_CAPABILITY_TREE_HPP
