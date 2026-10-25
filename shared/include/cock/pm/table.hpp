/**
 * @file table.hpp
 * @brief The table of processes of `pm`: kernel process id, process id of the table, parent, state and what
 * `pm` still has to tell the root server about the process.
 *
 * The process id of the table is not the kernel one. The kernel numbers processes with a generation counter, so
 * an id is never reused soon; the table numbers them from `FIRST_FREE_PID` in the order they arrive and keeps
 * `INIT_PID` for `init`. Nothing here allocates: the rows are an array, and a row of a process that has exited
 * and was collected is reused only when no row is free; a zombie, which a parent has not collected, is never
 * reused.
 */
#ifndef COCK_PM_TABLE_HPP
#define COCK_PM_TABLE_HPP

#include <cock/abi/pm.hpp>
#include <cock/abi/service.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::pm {

/** Most rows of the table, live and exited. */
constexpr size_t MAX_PROCESSES = 128;
/** Kernel process id of a row that holds none. */
constexpr uint32_t NO_KERNEL_ID = 0xFFFFFFFF;
/** Value of `Row::waitTicket` and `Row::waitTarget` while the process is not waiting. */
constexpr uint32_t NO_WAIT = 0;

/** One process. */
struct Row {
	/** Id of the process in the kernel, or `NO_KERNEL_ID` while the row is free. */
	uint32_t kernelId;
	/** Id of the process in the table, the one a program sees. */
	uint32_t pid;
	/** Id in the table of the process that asked for it, or `abi::pm::NO_PID`. */
	uint32_t parent;
	/** Whether the process runs. */
	abi::pm::State state;
	/** Status the process exited with; valid when `state` is `EXITED`. */
	uint32_t status;
	/** True when the exit notice must be sent on to the root server. */
	bool forward;
	/** True when the exit notice was not delivered yet and the root server must be told again. */
	bool forwardPending;
	/** Service label the process was created with, for the notice that it exists; `NO_SERVICE` when the root server asked for it and needs no notice. */
	uint32_t label;
	/** True when the root server was not yet told that the process exists. */
	bool announcePending;
	/** True while the process waits in a `WAIT` call that `pm` detached. */
	bool waiting;
	/** Ticket of the detached call; valid while `waiting`. */
	uint32_t waitTicket;
	/** Process id of the child the process waits for, or `abi::pm::ANY_CHILD`; valid while `waiting`. */
	uint32_t waitTarget;
	/** True when the root server was not yet told that the process is a copy made by `fork`. */
	bool forkPending;
};

/** The rows and the counter of process ids. */
class Table {
public:
	Table() { clear(); }

	/**
	 * @brief Empties the table; the next process id is `FIRST_FREE_PID` and there is no reaper.
	 */
	void clear() {
		for (Row &row : rows) release(row);
		nextPid = abi::pm::FIRST_FREE_PID;
		reaper = abi::pm::NO_PID;
		used = 0;
	}

	/**
	 * @brief Adds a running process.
	 *
	 * Takes a free row, or the oldest row of a process that has exited. A kernel id that is already in the
	 * table is refused, so a repeated request cannot make two rows for one process.
	 *
	 * @param kernelId Id of the process in the kernel.
	 * @param parent Id in the table of the process that asked for it, or `NO_PID`.
	 * @param forward True to send the exit notice on to the root server.
	 * @param wantedPid A process id to give it (`INIT_PID` for `init`), or `NO_PID` for the next free one. A
	 *        wanted id that a running process holds is refused; one that a process that has exited holds is
	 *        taken over, so a restarted `init` is `INIT_PID` again.
	 * @param announceLabel A service label to tell the root server about, or `NO_SERVICE` for none.
	 * @return The row, or null when the table is full of live processes, the kernel id is known or the
	 *         wanted id is taken.
	 */
	Row *add(uint32_t kernelId, uint32_t parent, bool forward, uint32_t wantedPid = abi::pm::NO_PID,
			 uint32_t announceLabel = abi::service::NO_SERVICE) {
		if (kernelId == NO_KERNEL_ID || findKernel(kernelId) != nullptr) return nullptr;
		if (wantedPid != abi::pm::NO_PID) {
			Row *holder = find(wantedPid);
			if (holder != nullptr && holder->state == abi::pm::State::RUNNING) return nullptr;
			if (holder != nullptr) {
				release(*holder);
				used--;
			}
		}

		Row *row = freeRow();
		if (row == nullptr) return nullptr;
		if (row->state == abi::pm::State::FREE) used++;
		*row = Row{kernelId,
				   wantedPid != abi::pm::NO_PID ? wantedPid : nextPid++,
				   parent,
				   abi::pm::State::RUNNING,
				   0,
				   forward,
				   false,
				   announceLabel,
				   announceLabel != abi::service::NO_SERVICE,
					   false,
					   NO_WAIT,
					   NO_WAIT,
					   false};
		return row;
	}

	/**
	 * @brief Says which process adopts the children of a process that exits, and whose children nobody collects.
	 * @param pid Process id of the root server in the table.
	 */
	void setReaper(uint32_t pid) { reaper = pid; }

	/**
	 * @brief Tells whether `add` would find a row for a new process.
	 * @return True when a row is free or one of a process that has exited can be reused.
	 */
	bool hasRoom() { return freeRow() != nullptr; }

	/**
	 * @brief Finds the row of a process by its kernel id.
	 * @param kernelId Id in the kernel.
	 * @return The row, or null.
	 */
	Row *findKernel(uint32_t kernelId) {
		for (Row &row : rows)
			if (row.state != abi::pm::State::FREE && row.kernelId == kernelId) return &row;
		return nullptr;
	}

	/**
	 * @brief Finds the row of a process by its id in the table.
	 * @param pid Id in the table.
	 * @return The row, or null.
	 */
	Row *find(uint32_t pid) {
		for (Row &row : rows)
			if (row.state != abi::pm::State::FREE && row.pid == pid) return &row;
		return nullptr;
	}

	/**
	 * @brief Records that a process exited.
	 *
	 * The row becomes `ZOMBIE` when its parent lives and is not the reaper, so the parent can collect the
	 * status; otherwise it becomes `EXITED` at once. The children of the process pass to the reaper, and those
	 * of them that were zombies become `EXITED`, since the reaper collects them.
	 *
	 * @param kernelId Id in the kernel.
	 * @param status Exit status from the notice.
	 * @return The row, or null when the process is not in the table or was already marked.
	 */
	Row *markExited(uint32_t kernelId, uint32_t status) {
		Row *row = findKernel(kernelId);
		if (row == nullptr || row->state == abi::pm::State::EXITED || row->state == abi::pm::State::ZOMBIE)
			return nullptr;
		row->state = hasCollector(row->parent) ? abi::pm::State::ZOMBIE : abi::pm::State::EXITED;
		row->status = status;
		row->forwardPending = row->forward;
		row->waiting = false;
		adoptChildren(row->pid);
		return row;
	}

	/**
	 * @brief Finds a child of a process that exited and was not collected.
	 * @param parent Process id of the parent in the table.
	 * @param target Process id of the child, or `abi::pm::ANY_CHILD`.
	 * @return The zombie row, or null.
	 */
	Row *findZombie(uint32_t parent, uint32_t target) {
		for (Row &row : rows)
			if (row.state == abi::pm::State::ZOMBIE && row.parent == parent && matches(row, target)) return &row;
		return nullptr;
	}

	/**
	 * @brief Tells whether a process has a child it could still collect: a live one or a zombie.
	 * @param parent Process id of the parent in the table.
	 * @param target Process id of the child, or `abi::pm::ANY_CHILD`.
	 * @return True when one exists; always false for the reaper, whose children nobody collects.
	 */
	bool hasChild(uint32_t parent, uint32_t target) {
		if (parent == abi::pm::NO_PID || parent == reaper) return false;
		for (Row &row : rows)
			if (row.state != abi::pm::State::FREE && row.state != abi::pm::State::EXITED &&
				row.parent == parent && matches(row, target))
				return true;
		return false;
	}

	/**
	 * @brief Collects a zombie: its status was handed to the parent, so the row is gone.
	 * @param row A row of the table; one that is not a zombie is left as it is.
	 */
	void collect(Row &row) {
		if (row.state == abi::pm::State::ZOMBIE) row.state = abi::pm::State::EXITED;
	}

	/**
	 * @brief Reads a row by position.
	 * @param index Position among the rows in use, from 0 to `count() - 1`.
	 * @return The row, or null past the last one.
	 */
	const Row *at(size_t index) const {
		size_t seen = 0;
		for (const Row &row : rows) {
			if (row.state == abi::pm::State::FREE) continue;
			if (seen++ == index) return &row;
		}
		return nullptr;
	}

	/**
	 * @brief Tells how many rows are in use, live or exited.
	 * @return The count.
	 */
	size_t count() const { return used; }

	/**
	 * @brief Finds a row whose exit notice still has to reach the root server.
	 * @return The row, or null when none waits.
	 */
	Row *pendingForward() {
		for (Row &row : rows)
			if (row.state == abi::pm::State::EXITED && row.forwardPending) return &row;
		return nullptr;
	}

	/**
	 * @brief Finds a row whose creation the root server was not yet told about.
	 * @return The row, or null when none waits.
	 */
	Row *pendingAnnounce() {
		for (Row &row : rows)
			if (row.state != abi::pm::State::FREE && row.announcePending) return &row;
		return nullptr;
	}

	/**
	 * @brief Finds a row of a copy made by `fork` whose creation the root server was not yet told about.
	 * @return The row, or null when none waits.
	 */
	Row *pendingFork() {
		for (Row &row : rows)
			if (row.state != abi::pm::State::FREE && row.forkPending) return &row;
		return nullptr;
	}

private:
	static void release(Row &row) {
		row = Row{NO_KERNEL_ID, abi::pm::NO_PID, abi::pm::NO_PID, abi::pm::State::FREE,
				  0,            false,           false,           abi::service::NO_SERVICE,
				  false,        false,           NO_WAIT,         NO_WAIT,
				  false};
	}

	static bool matches(const Row &row, uint32_t target) {
		return target == abi::pm::ANY_CHILD || row.pid == target;
	}

	bool hasCollector(uint32_t parent) {
		if (parent == abi::pm::NO_PID || parent == reaper) return false;
		Row *row = find(parent);
		return row != nullptr && row->state != abi::pm::State::EXITED && row->state != abi::pm::State::ZOMBIE;
	}

	void adoptChildren(uint32_t pid) {
		for (Row &row : rows) {
			if (row.state == abi::pm::State::FREE || row.parent != pid) continue;
			row.parent = reaper;
			collect(row);
		}
	}

	Row *freeRow() {
		Row *oldest = nullptr;
		for (Row &row : rows) {
			if (row.state == abi::pm::State::FREE) return &row;
			if (oldest == nullptr && row.state == abi::pm::State::EXITED && !row.forwardPending &&
				!row.announcePending)
				oldest = &row;
		}
		return oldest;
	}

	Row rows[MAX_PROCESSES];
	uint32_t reaper = abi::pm::NO_PID;
	uint32_t nextPid;
	size_t used;
};

} // namespace cock::pm

#endif // !COCK_PM_TABLE_HPP
