/** @file
 * @brief The table of endpoints: numbered mailboxes that name a receiving process
 * and outlive it. A fixed table, no allocation.
 */
#ifndef CORE_TASK_ENDPOINT_TABLE_HPP
#define CORE_TASK_ENDPOINT_TABLE_HPP

#include <cock/abi/endpoint.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

/**
 * @brief The endpoints of the system.
 *
 * An endpoint is created once and never destroyed. At any moment it is bound to at most one
 * process, the one whose first thread receives what is sent to it. When that process ends the
 * endpoint becomes unbound and a later bind gives it to the process that replaces it, so the
 * handles that clients hold keep working across a restart.
 *
 * Callers run with interrupts off (system calls) or hold an interrupt guard.
 */
class EndpointTable {
  public:
	/** @brief Capacity of the table. */
	static constexpr size_t MAX_ENDPOINTS = abi::endpoint::MAX_ENDPOINTS;

	/**
	 * @brief Creates an endpoint, unbound.
	 * @return Its id, from `FIRST_ENDPOINT` on in order, or `NO_ENDPOINT` when the table
	 * is full.
	 */
	uint32_t create() {
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++) {
			if (slots[idx].used) continue;
			slots[idx].used = true;
			slots[idx].owner = NO_OWNER;
			return static_cast<uint32_t>(idx) + abi::endpoint::FIRST_ENDPOINT;
		}
		return abi::endpoint::NO_ENDPOINT;
	}

	/**
	 * @brief Tells whether an id names an endpoint that was created.
	 * @param id Endpoint id.
	 * @return True for a created endpoint; false for `NO_ENDPOINT` and for an id outside the
	 * table.
	 */
	bool exists(uint32_t id) const { return locate(id) != nullptr; }

	/**
	 * @brief Binds an endpoint to a process, replacing the previous owner.
	 * @param id Endpoint id.
	 * @param owner Id of the process that receives from now on; not zero.
	 * @return True when bound. False when the endpoint does not exist or `owner` is zero.
	 */
	bool bind(uint32_t id, uint32_t owner) {
		Slot *slot = locate(id);
		if (slot == nullptr || owner == NO_OWNER) return false;
		slot->owner = owner;
		return true;
	}

	/**
	 * @brief Finds the process an endpoint is bound to.
	 * @param id Endpoint id.
	 * @return The process id, or zero when the endpoint does not exist or is unbound.
	 */
	uint32_t ownerOf(uint32_t id) const {
		const Slot *slot = locate(id);
		return slot == nullptr ? NO_OWNER : slot->owner;
	}

	/**
	 * @brief Unbinds every endpoint bound to a process that ends.
	 * @param owner Id of the process.
	 */
	void releaseOwner(uint32_t owner) {
		if (owner == NO_OWNER) return;
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++) {
			if (slots[idx].used && slots[idx].owner == owner) slots[idx].owner = NO_OWNER;
		}
	}

	/**
	 * @brief Removes an endpoint that was just created, when the step that should have
	 * followed its creation failed.
	 * @param id Endpoint id.
	 * @return True when the endpoint existed.
	 */
	bool destroy(uint32_t id) {
		Slot *slot = locate(id);
		if (slot == nullptr) return false;
		*slot = Slot();
		return true;
	}

	/** @brief Removes every endpoint. Used by the tests. */
	void clear() {
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++)
			slots[idx] = Slot();
	}

  private:
	/** Owner value of an unbound endpoint: no process has the id zero. */
	static constexpr uint32_t NO_OWNER = 0;

	struct Slot {
		bool used = false; /**< True once created. */
		uint32_t owner = NO_OWNER; /**< Process bound to the endpoint. */
	};

	/**
	 * @brief Finds the slot of an id.
	 * @param id Endpoint id.
	 * @return The slot, or null for `NO_ENDPOINT`, an id outside the table or a slot not created.
	 */
	const Slot *locate(uint32_t id) const {
		if (id < abi::endpoint::FIRST_ENDPOINT) return nullptr;
		size_t idx = id - abi::endpoint::FIRST_ENDPOINT;
		if (idx >= MAX_ENDPOINTS || !slots[idx].used) return nullptr;
		return &slots[idx];
	}
	Slot *locate(uint32_t id) { return const_cast<Slot *>(static_cast<const EndpointTable *>(this)->locate(id)); }

	Slot slots[MAX_ENDPOINTS]; /**< The table. */
};

/** @brief The one table of the system. */
inline constinit EndpointTable endpoint_table;

} // namespace cock::core::task

#endif // !CORE_TASK_ENDPOINT_TABLE_HPP
