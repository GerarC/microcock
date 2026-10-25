/** @file
 * @brief Replacing the program image of a caller with the image in the object it filled in.
 */
#ifndef PM_EXEC_HPP
#define PM_EXEC_HPP

#include <cock/abi/pm.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Replaces the image of the caller, which waits for the reply of this very call.
 *
 * Copies the `ExecRequest` out of the object first, so a thread of the caller that writes to it meanwhile
 * changes nothing `pm` checked. Refuses a request whose name does not end within `NAME_BYTES`, an image of no
 * bytes or one that does not fit in the object (the size of the object is `pm`'s own), an image that is not a
 * loadable ELF below `abi::exec::IMAGE_LIMIT`, and arguments that fail `cock::exec::valid`. Then builds the new
 * image as a pending process, writes the argument page into it and asks the kernel to put it in place of the
 * old one (`proc_exec`); the other threads of the caller end there. The object is given back whatever the result.
 *
 * @param caller Kernel process id of the caller, the badge of its handle.
 * @param id Object id the caller names; it must be the one `allocateObject` gave it.
 * @param replaced Set to true when the kernel replaced the image. The call of the caller is closed then and
 * must not be answered.
 * @return `OK` when replaced; `NOT_FOUND` when the caller has no live row; `BAD_REQUEST` for an object the caller
 * does not hold or a request that fails the checks above; `NO_ROOM` when the object cannot be mapped; `FAILED`
 * when the loader or the kernel refused (no process slot, no memory, a caller that is not the one `pm` serves).
 * The old image runs on after every status but `OK`.
 */
cock::abi::pm::Status execFromObject(uint32_t caller, uint32_t id, bool &replaced);

} // namespace cock::pm

#endif // !PM_EXEC_HPP
