/** @file
 * @brief `WAIT`: a parent collects the status of a child that exited, and `pm` keeps the call of a parent whose
 * children still run until one of them exits.
 */
#ifndef PM_WAIT_HPP
#define PM_WAIT_HPP

#include <cock/abi/pm.hpp>
#include <cock/pm/table.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Serves a `WAIT` of `caller`.
 *
 * A child of the caller that is a zombie and matches `target` is collected at once: the reply carries its
 * process id in word 1 and its exit status in word 2, and the row becomes `EXITED`. When no zombie matches but a
 * live child does, the call is detached with `ipc_defer` and the caller stays blocked until `answerWaiter` finds
 * the child that exited.
 *
 * @param caller Badge of the handle: the kernel process id of the caller.
 * @param target Process id of the table of the child to wait for, or `abi::pm::ANY_CHILD`.
 * @param reply Receives the values when the answer is immediate.
 * @param detached Set to true when the call was detached and must not be answered now.
 * @return `OK` for an answer or a detached call, `NOT_FOUND` when the caller is not in the table or has no
 *         such child (the root server never has one), `FAILED` when the call could not be detached.
 */
cock::abi::pm::Status waitForChild(uint32_t caller, uint32_t target,
								   uint32_t (&reply)[cock::abi::pm::CALL_WORDS], bool &detached);

/**
 * @brief Answers the parent of a process that just exited, when it waits for that process.
 *
 * Does nothing unless the row is a zombie and its parent has a detached `WAIT` that matches. When the answer
 * is delivered the row becomes `EXITED` and the parent stops waiting; when it cannot be delivered (the parent
 * gave up or died) the zombie stays for the next `WAIT`.
 *
 * @param exited The row `Table::markExited` returned.
 */
void answerWaiter(Row &exited);

} // namespace cock::pm

#endif // !PM_WAIT_HPP
