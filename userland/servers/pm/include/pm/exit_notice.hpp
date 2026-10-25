/** @file
 * @brief The exit notices the kernel sends `pm`, and the notices `pm` sends on to the root server.
 */
#ifndef PM_EXIT_NOTICE_HPP
#define PM_EXIT_NOTICE_HPP

#include <cock/ipc/message.hpp>

namespace cock::pm {

/** Longest wait, in milliseconds, between two tries to deliver a notice to the root server that had no room. */
constexpr uint32_t NOTICE_RETRY_MILLISECONDS = 50;

/**
 * @brief Takes in one asynchronous message.
 *
 * A `PROCESS_EXITED` notice marks the row of the process as exited and, when the process asked for it with
 * `FORWARD_EXIT`, sends the root server a `PM_EXITED` message with the same two numbers. A notice for a process
 * that is not in the table is ignored. A `PM_FORK_SETTLED` message from the root server (the badge is checked)
 * answers the parent of the copy (`settleFork`). When the parent of the process waits for it (`WAIT`), its detached call is
 * answered and the status is collected. Any other message is dropped.
 *
 * @param message A message from `ipc_try_recv`.
 */
void takeMessage(const cock::ipc::Message &message);

/**
 * @brief Delivers the notices the root server had no room for: the exits that must be forwarded, the
 * `PM_SPAWNED` message of each process `init` asked for, and the `PM_FORKED` message of each copy made by `fork`
 * (kernel ids of the copy and of its parent; a copy whose parent has no row any more is dropped).
 *
 * Stops at the first one that still does not fit; its row keeps the mark and the next call tries it again.
 *
 * @return True while a notice is waiting, or a parent waits for the acknowledgement of a copy (`holdParent`), so the
 * caller wakes up again within `NOTICE_RETRY_MILLISECONDS`.
 */
bool flushNotices();

} // namespace cock::pm

#endif // !PM_EXIT_NOTICE_HPP
