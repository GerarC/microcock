/** @file
 * @brief Creating a process from the object a caller filled in.
 */
#ifndef PM_SPAWN_HPP
#define PM_SPAWN_HPP

#include <cock/abi/pm.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Creates a process from the request and the image in an object `pm` made for the caller.
 *
 * Copies the `SpawnRequest` out of the object first, so a thread of the caller that writes to it meanwhile
 * changes nothing `pm` checked. Refuses a request whose name does not end within `NAME_BYTES`, with flags `pm`
 * does not know, with an image of no bytes or one that does not fit in the object (the size of the object is
 * `pm`'s own, never the caller's), with a label outside the program labels, or with more IPC labels than
 * `MAX_SPAWN_IPC`. The kernel checks the rights again against what `pm` holds. The object is unmapped and
 * given back whatever the result, so a caller must ask again for the next process.
 *
 * @param caller Kernel process id of the caller, the badge of its handle.
 * @param id Object id the caller names; it must be the one `allocateObject` gave it.
 * @param kernelId Receives the kernel process id of the new process.
 * @param pid Receives the process id of the table.
 * @return `OK`; `BAD_REQUEST` for an object the caller does not hold or a request that fails the checks above;
 * `NO_ROOM` when the table is full of live processes or the object cannot be mapped; `FAILED` when the kernel
 * refused the process (not a loadable image, rights it cannot give, no memory or no process id).
 */
cock::abi::pm::Status spawnFromObject(uint32_t caller, uint32_t id, uint32_t &kernelId, uint32_t &pid);

} // namespace cock::pm

#endif // !PM_SPAWN_HPP
