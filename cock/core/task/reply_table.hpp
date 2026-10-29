/** @file
 * @brief The table of reply tokens: one-shot rights to answer a message, kept by the kernel so
 * they take no entry of the capability table of the server. A fixed table, no allocation.
 */
#ifndef CORE_TASK_REPLY_TABLE_HPP
#define CORE_TASK_REPLY_TABLE_HPP

#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

/**
 * @brief The open reply tokens of the system.
 *
 * A token is opened when a message is sent through a handle with `SEND_EXPECT_REPLY`. It names the
 * thread that asked and the process that may answer, the holder. The holder can answer once
 * (`close` follows), or pass the token to another process with a forwarded message, which becomes
 * the holder. Nobody else can use it, and a token that was closed never works again: its number
 * carries a generation that changes every time the slot is reused.
 *
 * A token is dropped when the holder or the process that asked ends, and a process that asked
 * can have `MAX_REPLIES_PER_REQUESTER` open at once, so a server that never answers cannot use up
 * the table.
 *
 * Callers run with interrupts off (system calls) or hold an interrupt guard.
 */
class ReplyTable {
  public:
	/** @brief Capacity of the table. */
	static constexpr size_t MAX_REPLIES = abi::endpoint::MAX_REPLIES;

	/**
	 * @brief Opens a token.
	 * @param holder Id of the process that may answer; not zero.
	 * @param requester_thread Id of the thread that asked, which receives the answer; not zero.
	 * @param requester_process Id of the process of that thread.
	 * @return The token (never `NO_REPLY`), or `NO_REPLY` when the table is full, the requester
	 * has `MAX_REPLIES_PER_REQUESTER` open or an id is zero.
	 */
	uint32_t open(uint32_t holder, uint32_t requester_thread, uint32_t requester_process) {
		if (holder == NO_ID) return abi::endpoint::NO_REPLY;
		return openFor(holder, requester_thread, requester_process);
	}

	/**
	 * @brief Opens a token for a message that waits in the queue of an endpoint, so nobody holds it yet.
	 *
	 * The process that takes the message from the queue becomes the holder (`claim`). Until then
	 * only the process that asked can lose it, by ending.
	 *
	 * @param requester_thread Id of the thread that asked, which receives the answer; not zero.
	 * @param requester_process Id of the process of that thread; not zero.
	 * @return The token, or `NO_REPLY` under the same conditions as `open`.
	 */
	uint32_t openQueued(uint32_t requester_thread, uint32_t requester_process) {
		return openFor(NO_ID, requester_thread, requester_process);
	}

	/**
	 * @brief Takes a token back from its holder because the message that carries it goes into a queue.
	 * @param token Token number.
	 * @param from Id of the current holder.
	 * @return True when `from` held the token and nobody holds it now.
	 */
	bool park(uint32_t token, uint32_t from) {
		Slot *slot = locate(token);
		if (slot == nullptr || from == NO_ID || slot->holder != from) return false;
		slot->holder = NO_ID;
		return true;
	}

	/**
	 * @brief Gives a token that nobody holds to the process that took its message from a queue.
	 * @param token Token number.
	 * @param holder Id of the process; not zero.
	 * @return True when the token was open and held by nobody.
	 */
	bool claim(uint32_t token, uint32_t holder) {
		Slot *slot = locate(token);
		if (slot == nullptr || holder == NO_ID || slot->holder != NO_ID) return false;
		slot->holder = holder;
		return true;
	}

	/**
	 * @brief Tells whether a process holds a token.
	 * @param token Token number.
	 * @param holder Id of the process.
	 * @return True when the token is open and `holder` may answer it.
	 */
	bool holds(uint32_t token, uint32_t holder) const {
		const Slot *slot = locate(token);
		return slot != nullptr && slot->holder == holder;
	}

	/**
	 * @brief Finds the thread that asked.
	 * @param token Token number.
	 * @return The thread id, or zero when the token is not open.
	 */
	uint32_t requesterOf(uint32_t token) const {
		const Slot *slot = locate(token);
		return slot == nullptr ? NO_ID : slot->requesterThread;
	}

	/**
	 * @brief Gives a token to another process, as a forwarded message does.
	 * @param token Token number.
	 * @param from Id of the current holder.
	 * @param to Id of the new holder; not zero.
	 * @return True when `from` held the token and it now belongs to `to`.
	 */
	bool pass(uint32_t token, uint32_t from, uint32_t to) {
		Slot *slot = locate(token);
		if (slot == nullptr || slot->holder != from || to == NO_ID) return false;
		slot->holder = to;
		return true;
	}

	/**
	 * @brief Closes a token, after it was used or when its message could not be delivered.
	 * @param token Token number.
	 * @return True when the token was open.
	 */
	bool close(uint32_t token) {
		Slot *slot = locate(token);
		if (slot == nullptr) return false;
		release(*slot);
		return true;
	}

	/**
	 * @brief Drops every token that a process that ends holds or asked for.
	 * @param process Id of the process.
	 */
	void releaseProcess(uint32_t process) {
		if (process == NO_ID) return;
		for (Slot &slot : slots) {
			if (slot.used && (slot.holder == process || slot.requesterProcess == process))
				release(slot);
		}
	}

	/** @return The number of open tokens. */
	size_t openCount() const {
		size_t count = 0;
		for (const Slot &slot : slots)
			if (slot.used) count++;
		return count;
	}

	/** @brief Closes every token. Used by the tests. */
	void clear() {
		for (Slot &slot : slots)
			if (slot.used) release(slot);
	}

  private:
	/** Id value that names nobody: no process or thread has the id zero. */
	static constexpr uint32_t NO_ID = 0;
	/** Bits of a token that hold the index of its slot. */
	static constexpr uint32_t INDEX_BITS = 8;
	/** Mask of the index in a token. */
	static constexpr uint32_t INDEX_MASK = (uint32_t(1) << INDEX_BITS) - 1;
	/** Generation a slot has before its first use; zero is never a generation, so no token is zero. */
	static constexpr uint32_t FIRST_GENERATION = 1;
	/** Largest generation, which leaves the token 32 bits wide. */
	static constexpr uint32_t LAST_GENERATION = (uint32_t(1) << (32 - INDEX_BITS)) - 1;

	static_assert((size_t(1) << INDEX_BITS) == MAX_REPLIES, "the token index must address the whole table");

	struct Slot {
		bool used = false; /**< True while the token is open. */
		uint32_t generation = FIRST_GENERATION; /**< Changes every time the slot is freed. */
		uint32_t holder = NO_ID; /**< Process that may answer. */
		uint32_t requesterThread = NO_ID; /**< Thread that receives the answer. */
		uint32_t requesterProcess = NO_ID; /**< Process of that thread. */
	};

	/**
	 * @brief Opens a token, whoever holds it.
	 * @param holder Id of the holder, or zero for a token that waits in a queue.
	 * @param requester_thread Id of the thread that asked; not zero.
	 * @param requester_process Id of the process of that thread; not zero.
	 * @return The token, or `NO_REPLY` when the table is full, the requester has
	 * `MAX_REPLIES_PER_REQUESTER` open or an id is zero.
	 */
	uint32_t openFor(uint32_t holder, uint32_t requester_thread, uint32_t requester_process) {
		if (requester_thread == NO_ID || requester_process == NO_ID) return abi::endpoint::NO_REPLY;

		size_t asked = 0;
		Slot *free = nullptr;
		for (Slot &slot : slots) {
			if (!slot.used) {
				if (free == nullptr) free = &slot;
			} else if (slot.requesterProcess == requester_process) {
				asked++;
			}
		}
		if (free == nullptr || asked >= abi::endpoint::MAX_REPLIES_PER_REQUESTER)
			return abi::endpoint::NO_REPLY;

		free->used = true;
		free->holder = holder;
		free->requesterThread = requester_thread;
		free->requesterProcess = requester_process;
		return tokenOf(*free);
	}

	/**
	 * @brief Builds the number of the token of a slot.
	 * @param slot A slot of the table.
	 * @return Its generation and index.
	 */
	uint32_t tokenOf(const Slot &slot) const {
		return (slot.generation << INDEX_BITS) | static_cast<uint32_t>(&slot - slots);
	}

	/**
	 * @brief Finds the open slot of a token.
	 * @param token Token number.
	 * @return The slot, or null when the token is zero, was closed or has an old generation.
	 */
	const Slot *locate(uint32_t token) const {
		const Slot &slot = slots[token & INDEX_MASK];
		if (!slot.used || token != tokenOf(slot)) return nullptr;
		return &slot;
	}
	Slot *locate(uint32_t token) {
		return const_cast<Slot *>(static_cast<const ReplyTable *>(this)->locate(token));
	}

	/**
	 * @brief Frees a slot and moves it to its next generation.
	 * @param slot An open slot.
	 */
	static void release(Slot &slot) {
		uint32_t next = slot.generation == LAST_GENERATION ? FIRST_GENERATION : slot.generation + 1;
		slot = Slot();
		slot.generation = next;
	}

	Slot slots[MAX_REPLIES]; /**< The table. */
};

/** @brief The one table of the system. */
inline constinit ReplyTable reply_table;

/**
 * @brief Closes the reply token of a queued message that the kernel throws away.
 * @param message The message; nothing happens when it carries no token.
 */
inline void closeTokenOfMessage(const ipc::Message &message) {
	if (message.reply != abi::endpoint::NO_REPLY) reply_table.close(message.reply);
}

} // namespace cock::core::task

#endif // !CORE_TASK_REPLY_TABLE_HPP
