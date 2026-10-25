/** @file
 * @brief The requests `pm` serves and who may make each.
 */
#ifndef PM_REQUESTS_HPP
#define PM_REQUESTS_HPP

#include <cock/abi/pm.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Serves one synchronous call.
 *
 * `ALLOCATE`, `SPAWN` and `REGISTER` are served only for the root server, whose handle carries the badge
 * `ROOT_PID`; anyone else gets `DENIED`. `LIST` is served for every caller that holds a handle; for a process that lives it asks the kernel for the states of
 * its threads and answers `RUNNING` or `SLEEPING` (the row keeps `RUNNING`), and falls back to `RUNNING` when the
 * kernel cannot read them. A `REGISTER`
 * refuses a process the kernel does not know as alive and a parent that is not in the table.
 *
 * @param caller Badge of the handle the call came through: the kernel process id of the caller.
 * @param words The four request words.
 * @param reply Receives the four reply words; word 0 holds the status.
 */
void serve(uint32_t caller, const uint32_t (&words)[cock::abi::pm::CALL_WORDS],
		   uint32_t (&reply)[cock::abi::pm::CALL_WORDS]);

} // namespace cock::pm

#endif // !PM_REQUESTS_HPP
