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

} // namespace cock::pm

#endif // !PM_FORK_HPP
