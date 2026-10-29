/** @file
 * @brief Processes that were created by a loader in user space and have no
 * thread yet.
 */
#ifndef CORE_TASK_PENDING_PROCESS_HPP
#define CORE_TASK_PENDING_PROCESS_HPP

#include <cock/abi/process.hpp>
#include <cock/abi/service.hpp>
#include <cock/core/task/process.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

/**
 * @brief One process between `SYS_PROC_CREATE` and `SYS_PROC_START`.
 *
 * A process gets its id from its first thread, so until it starts it cannot be
 * named by a process id. The creator names it by the index of its entry in the
 * table, which only the creator may use. The table is global and small, so a
 * program cannot exhaust the process pool with half built processes, and an entry
 * never outlives its creator: when the creator is destroyed, so is everything it
 * left pending.
 */
class PendingProcesses {
  public:
	/** @brief Number of entries, the system wide limit of pending processes. */
	static constexpr size_t CAPACITY = abi::process::MAX_PENDING_PROCESSES;
	/** @brief Handle that names no entry. */
	static constexpr uint32_t NO_HANDLE = abi::process::NO_PENDING_PROCESS;

	/** @brief What the kernel keeps about a pending process. */
	struct Entry {
		Process *process; /**< The process, null when the entry is free. */
		const Process *owner; /**< The creator, the only one that may use the entry. */
		size_t pages; /**< Pages mapped so far, counted before the mapping is tried. */
		bool notify; /**< The creator asked for an exit notice. */
		bool hasRights; /**< `rights` holds the label and IPC labels to apply. */
		abi::service::SpawnRights rights; /**< Rights to apply at start. */
	};

	/**
	 * @brief Tells whether an owner may create one more pending process.
	 * @param owner The creator.
	 * @return True when the table has a free entry and the owner holds fewer than
	 * `MAX_PENDING_PER_PROCESS`.
	 */
	static bool canAdd(const Process *owner);
	/**
	 * @brief Records a new pending process.
	 * @param owner The creator.
	 * @param process The process, without threads, now owned by the table.
	 * @param notify True when the creator asked for an exit notice.
	 * @param rights The rights to apply at start, or null for none.
	 * @return The handle, or `NO_HANDLE` when `canAdd` would be false, in which
	 * case the process stays with the caller.
	 */
	static uint32_t add(const Process *owner, Process *process, bool notify,
						const abi::service::SpawnRights *rights);
	/**
	 * @brief Finds the entry of a handle.
	 * @param handle Any value.
	 * @param owner The process that uses the handle.
	 * @return The entry, or null when the handle is out of range, free or belongs to
	 * another process.
	 */
	static Entry *find(uint32_t handle, const Process *owner);
	/**
	 * @brief Frees an entry without touching the process, which has a thread now.
	 * @param handle Handle of a used entry.
	 */
	static void release(uint32_t handle);
	/**
	 * @brief Destroys the process of an entry, with its address space, and frees
	 * the entry.
	 * @param handle Handle of a used entry.
	 */
	static void discard(uint32_t handle);
	/**
	 * @brief Destroys every pending process of an owner.
	 * @param owner The creator that is being destroyed.
	 */
	static void discardOwnedBy(const Process *owner);

  private:
	static Entry entries[CAPACITY]; /**< The table. */
};

} // namespace cock::core::task

#endif // !CORE_TASK_PENDING_PROCESS_HPP
