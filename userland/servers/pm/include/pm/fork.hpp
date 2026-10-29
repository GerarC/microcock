/** @file
 * @brief Making a copy of a caller that waits for the reply of its own `FORK` call.
 */
#ifndef PM_FORK_HPP
#define PM_FORK_HPP

#include <cock/abi/pm.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Asks the kernel for a copy of the caller and puts it in the table.
 *
 * The caller waits for the reply of this very call, which is the consent the kernel asks for. The copy
 * resumes inside the same call with a reply that carries `OK` and `NO_PID`, so it knows it is the child; the
 * answer to the caller, with the process id of the child in the table, is built by the caller of this
 * function and sent by the loop of `pm`. The copy is a child of the caller in the table and `pm` is its parent
 * in the kernel, so the exit notice reaches `pm`.
 *
 * @param caller Kernel process id of the caller, the badge of its handle.
 * @param pid Receives the process id in the table of the child, valid for `OK` only.
 * @return `OK` when the copy exists and has its row; `NOT_FOUND` when the caller has no live row; `NO_ROOM`
 * when the table is full (the kernel is not asked); `FAILED` when the kernel refused. Nothing is created
 * after any status but `OK`.
 */
cock::abi::pm::Status forkProcess(uint32_t caller, uint32_t &pid);

/** Number of timeouts of the wait for the root server (`NOTICE_RETRY_MILLISECONDS` each) after which `pm` answers the parent of a copy without the acknowledgement. */
constexpr uint32_t MAX_SETTLE_TIMEOUTS = 40;

/**
 * @brief Keeps the call of the parent of a copy until the root server says the copy is settled.
 *
 * The copy runs from the moment the kernel makes it, but the parent stays blocked in its `FORK` call, so it cannot
 * open, close or write anything the copy was meant to inherit before the programs that keep state for each process
 * have copied it. The call is detached with `ipc_defer`, and `settleFork` answers it.
 *
 * @param caller Kernel process id of the parent, the badge of its handle.
 * @param child Process id in the table of the copy.
 * @return true when the call was detached and must not be answered now; false when the parent has no row or the
 * kernel gave no ticket, and the answer goes at once, with no wait.
 */
bool holdParent(uint32_t caller, uint32_t child);

/**
 * @brief Answers the parent of a copy: the root server says every program that keeps state copied it.
 *
 * Does nothing when the copy has no row, or when its parent does not wait (the wait timed out first).
 *
 * @param childKernelId Kernel process id of the copy, from `PM_FORK_SETTLED`.
 */
void settleFork(uint32_t childKernelId);

/**
 * @brief Counts one timeout of the wait for the root server and answers the parents that waited too long.
 *
 * A parent that waits `MAX_SETTLE_TIMEOUTS` times is answered as if the acknowledgement had come, so a root
 * server that is gone does not leave it blocked for ever.
 */
void ageSettling();

/**
 * @brief Tells whether any parent waits for a copy to be settled.
 * @return true while one does, so the loop of `pm` wakes up within `NOTICE_RETRY_MILLISECONDS`.
 */
bool anySettling();

} // namespace cock::pm

#endif // !PM_FORK_HPP
