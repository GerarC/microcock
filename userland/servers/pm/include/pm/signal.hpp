/** @file
 * @brief `KILL`: who may signal whom, and the call to the kernel that does it.
 */
#ifndef PM_SIGNAL_HPP
#define PM_SIGNAL_HPP

#include <cock/abi/pm.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Serves a `KILL` of `caller`.
 *
 * The root server may signal any process that lives; anyone else only a descendant of its own row (`Table::isDescendant`),
 * which excludes itself and its ancestors. The signal itself is the kernel's (`SYS_PROC_SIGNAL`, which `pm` may call
 * because it holds the `SPAWN` right): `pm` decides, the kernel delivers or ends the process. The row changes when the
 * exit notice arrives, not here.
 *
 * @param caller Badge of the handle: the kernel process id of the caller.
 * @param target Process id of the table of the process to signal.
 * @param signal `abi::process::SIGNAL_INTERRUPT`, `SIGNAL_TERMINATE` or `SIGNAL_KILL`.
 * @return `OK` when the kernel sent it; `BAD_REQUEST` for an unknown signal; `NOT_FOUND` when the target is not in the
 *         table or has ended; `DENIED` when the caller may not signal it; `FAILED` when the kernel refused.
 */
cock::abi::pm::Status signalProcess(uint32_t caller, uint32_t target, uint32_t signal);

} // namespace cock::pm

#endif // !PM_SIGNAL_HPP
