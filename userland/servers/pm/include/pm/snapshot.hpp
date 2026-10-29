/** @file
 * @brief The state of a row as a caller sees it, and the snapshot of the whole table in a shared object.
 */
#ifndef PM_SNAPSHOT_HPP
#define PM_SNAPSHOT_HPP

#include <cock/abi/pm.hpp>
#include <pm/state.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Gives the state `LIST` and `SNAPSHOT` report for a row.
 *
 * A row stored as `RUNNING` is reported as the kernel sees its threads at this moment (`SYS_PROC_STATE`): `RUNNING`,
 * `SLEEPING` or `STOPPED`; when the kernel cannot read them the stored state stays. Any other stored state is reported
 * as it is.
 *
 * @param row A row of the table.
 * @return The state.
 */
cock::abi::pm::State visibleState(const Row &row);

/**
 * @brief Writes the table into the shared object `pm` made for a caller.
 *
 * Writes an `abi::pm::Snapshot` at the start of the object: the number of rows and, for each, the ids, the state, the
 * exit status and the name. The mapping of `pm` is removed and its own share of the object is given back, so the
 * caller, which still holds its share, maps and reads it, and releases it. Nothing is read from the object.
 *
 * @param caller Kernel process id of the caller, the badge of its handle.
 * @param id Object id the caller names.
 * @param count Receives the number of rows written.
 * @return `OK`; `BAD_REQUEST` when the caller holds no object with that id, or one smaller than a `Snapshot` (the
 * object is given back); `NO_ROOM` when `pm` cannot map it (the object is given back).
 */
cock::abi::pm::Status takeSnapshot(uint32_t caller, uint32_t id, uint32_t &count);

} // namespace cock::pm

#endif // !PM_SNAPSHOT_HPP
