/** @file
 * @brief The table of endpoints: numbered mailboxes that name a receiving process
 * and outlive it, with the queue of messages that waits in each. A fixed table, no allocation.
 */
#ifndef CORE_TASK_ENDPOINT_TABLE_HPP
#define CORE_TASK_ENDPOINT_TABLE_HPP

#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
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
 * The asynchronous messages sent to an endpoint wait in a queue of the endpoint, not in the inbox
 * of a thread: they are taken by whichever process is bound when it receives, so what was sent
 * while a server was down is not lost. The queues draw their cells from one shared pool.
 *
 * Callers run with interrupts off (system calls) or hold an interrupt guard.
 */
class EndpointTable {
  public:
	/** @brief Capacity of the table. */
	static constexpr size_t MAX_ENDPOINTS = abi::endpoint::MAX_ENDPOINTS;

	/** @brief Called with every queued message that the table throws away, so the caller can undo what the message carried (a reply token). */
	using DropFn = void (*)(const ipc::Message &message);

	/** @brief Builds an empty table whose free cells form one chain. */
	constexpr EndpointTable() { resetCells(); }

	/**
	 * @brief Creates an endpoint, unbound.
	 * @return Its id, from `FIRST_ENDPOINT` on in order, or `NO_ENDPOINT` when the table
	 * is full.
	 */
	uint32_t create() {
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++) {
			if (slots[idx].used) continue;
			slots[idx] = Slot();
			slots[idx].used = true;
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
	 *
	 * The messages that carry a capability are thrown away when the owner changes: the kernel gave
	 * the capability to the process that was bound when they were sent, so they mean nothing to
	 * the next one. Every other queued message stays for the new owner.
	 *
	 * @param id Endpoint id.
	 * @param owner Id of the process that receives from now on; not zero.
	 * @param drop Called with each message thrown away; may be null.
	 * @return True when bound. False when the endpoint does not exist or `owner` is zero.
	 */
	bool bind(uint32_t id, uint32_t owner, DropFn drop = nullptr) {
		Slot *slot = locate(id);
		if (slot == nullptr || owner == NO_OWNER) return false;
		if (slot->owner != owner) dropTransfers(*slot, drop);
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
	 * @brief Unbinds every endpoint bound to a process that ends. The messages queued on them stay,
	 * except the ones that carry a capability.
	 * @param owner Id of the process.
	 * @param drop Called with each message thrown away; may be null.
	 */
	void releaseOwner(uint32_t owner, DropFn drop = nullptr) {
		if (owner == NO_OWNER) return;
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++) {
			if (!slots[idx].used || slots[idx].owner != owner) continue;
			dropTransfers(slots[idx], drop);
			slots[idx].owner = NO_OWNER;
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
		discardAll(*slot, nullptr);
		*slot = Slot();
		return true;
	}

	/**
	 * @brief Appends a message to the queue of an endpoint.
	 * @param id Endpoint id.
	 * @param message Message to copy in.
	 * @return True when queued. False when the endpoint does not exist, its queue holds
	 * `MAX_QUEUED_PER_ENDPOINT` messages or the shared pool of cells is empty.
	 */
	bool enqueue(uint32_t id, const ipc::Message &message) {
		Slot *slot = locate(id);
		if (slot == nullptr || slot->queued >= abi::endpoint::MAX_QUEUED_PER_ENDPOINT) return false;
		if (freeHead == NO_CELL) return false;

		uint16_t cell = freeHead;
		freeHead = cells[cell].next;
		cells[cell].message = message;
		cells[cell].next = NO_CELL;
		if (slot->tail == NO_CELL) {
			slot->head = cell;
		} else {
			cells[slot->tail].next = cell;
		}
		slot->tail = cell;
		slot->queued++;
		queuedTotal++;
		return true;
	}

	/**
	 * @brief Takes the oldest message of the queue of an endpoint.
	 * @param id Endpoint id.
	 * @param out Receives the message.
	 * @return False when the endpoint does not exist or its queue is empty.
	 */
	bool dequeue(uint32_t id, ipc::Message &out) {
		Slot *slot = locate(id);
		return slot != nullptr && pop(*slot, out);
	}

	/**
	 * @brief Takes the oldest message of the first endpoint bound to a process that has one queued.
	 * @param owner Id of the process.
	 * @param out Receives the message.
	 * @return False when no endpoint of the process has a message queued.
	 */
	bool take(uint32_t owner, ipc::Message &out) {
		if (queuedTotal == 0 || owner == NO_OWNER) return false;
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++) {
			if (slots[idx].used && slots[idx].owner == owner && pop(slots[idx], out)) return true;
		}
		return false;
	}

	/**
	 * @brief Tells whether an endpoint bound to a process has a message queued.
	 * @param owner Id of the process.
	 * @return True when `take` would succeed.
	 */
	bool hasQueuedFor(uint32_t owner) const {
		if (queuedTotal == 0 || owner == NO_OWNER) return false;
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++) {
			if (slots[idx].used && slots[idx].owner == owner && slots[idx].queued != 0) return true;
		}
		return false;
	}

	/**
	 * @brief Counts the messages queued on an endpoint.
	 * @param id Endpoint id.
	 * @return The count; zero for an endpoint that does not exist.
	 */
	size_t queuedCount(uint32_t id) const {
		const Slot *slot = locate(id);
		return slot == nullptr ? 0 : slot->queued;
	}

	/** @return The number of messages queued on all the endpoints. */
	size_t queuedMessages() const { return queuedTotal; }

	/** @brief Removes every endpoint and every queued message. Used by the tests. */
	void clear() {
		for (size_t idx = 0; idx < MAX_ENDPOINTS; idx++)
			slots[idx] = Slot();
		resetCells();
	}

  private:
	/** Owner value of an unbound endpoint: no process has the id zero. */
	static constexpr uint32_t NO_OWNER = 0;
	/** Cell index that ends a chain; one past the last cell. */
	static constexpr uint16_t NO_CELL = static_cast<uint16_t>(abi::endpoint::QUEUED_MESSAGE_POOL);

	static_assert(abi::endpoint::QUEUED_MESSAGE_POOL < UINT16_MAX, "a cell index must fit in 16 bits");

	struct Slot {
		bool used = false; /**< True once created. */
		uint32_t owner = NO_OWNER; /**< Process bound to the endpoint. */
		uint16_t head = NO_CELL; /**< Oldest queued message. */
		uint16_t tail = NO_CELL; /**< Newest queued message. */
		size_t queued = 0; /**< Messages in the queue. */
	};

	struct Cell {
		ipc::Message message{}; /**< The queued message. */
		uint16_t next = NO_CELL; /**< Next message of the queue, or next free cell. */
	};

	/** @brief Links every cell into the free chain and forgets the queued count. */
	constexpr void resetCells() {
		for (size_t idx = 0; idx < abi::endpoint::QUEUED_MESSAGE_POOL; idx++)
			cells[idx].next = static_cast<uint16_t>(idx + 1);
		freeHead = 0;
		queuedTotal = 0;
	}

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

	/**
	 * @brief Removes the oldest message of a queue and frees its cell.
	 * @param slot Endpoint slot.
	 * @param out Receives the message.
	 * @return False when the queue is empty.
	 */
	bool pop(Slot &slot, ipc::Message &out) {
		if (slot.head == NO_CELL) return false;
		uint16_t cell = slot.head;
		out = cells[cell].message;
		slot.head = cells[cell].next;
		if (slot.head == NO_CELL) slot.tail = NO_CELL;
		release(cell);
		slot.queued--;
		return true;
	}

	/**
	 * @brief Returns a cell to the free chain.
	 * @param cell Index of the cell.
	 */
	void release(uint16_t cell) {
		cells[cell].next = freeHead;
		freeHead = cell;
		queuedTotal--;
	}

	/**
	 * @brief Throws away every queued message that carries a capability.
	 * @param slot Endpoint slot.
	 * @param drop Called with each message removed; may be null.
	 */
	void dropTransfers(Slot &slot, DropFn drop) {
		uint16_t *link = &slot.head;
		uint16_t previous = NO_CELL;
		while (*link != NO_CELL) {
			uint16_t cell = *link;
			if (cells[cell].message.type != ipc::MessageType::CAPABILITY_TRANSFER) {
				previous = cell;
				link = &cells[cell].next;
				continue;
			}
			if (drop != nullptr) drop(cells[cell].message);
			*link = cells[cell].next;
			if (slot.tail == cell) slot.tail = previous;
			release(cell);
			slot.queued--;
		}
	}

	/**
	 * @brief Throws away every queued message of an endpoint.
	 * @param slot Endpoint slot.
	 * @param drop Called with each message removed; may be null.
	 */
	void discardAll(Slot &slot, DropFn drop) {
		ipc::Message message;
		while (pop(slot, message))
			if (drop != nullptr) drop(message);
	}

	Slot slots[MAX_ENDPOINTS]; /**< The table. */
	Cell cells[abi::endpoint::QUEUED_MESSAGE_POOL]; /**< The shared pool of queue cells. */
	uint16_t freeHead = 0; /**< First free cell. */
	size_t queuedTotal = 0; /**< Messages queued on every endpoint. */
};

/** @brief The one table of the system. */
inline constinit EndpointTable endpoint_table;

} // namespace cock::core::task

#endif // !CORE_TASK_ENDPOINT_TABLE_HPP
