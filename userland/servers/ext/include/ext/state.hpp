/** @file
 * @brief The `VALID` flag of the superblock, cleared while an operation changes the volume.
 */
#ifndef EXT_STATE_HPP
#define EXT_STATE_HPP

#include <cock/abi/fs.hpp>

namespace cock::ext {

/**
 * @brief Clears the `VALID` flag before a change.
 *
 * @return True on success, false on an I/O error.
 */
bool beginUpdate();

/**
 * @brief Writes the free counters and the write time and sets the `VALID` flag after a change.
 *
 * @param status The status of the change.
 * @return `status`, or `IO_ERROR` when the superblock cannot be written.
 */
cock::abi::fs::Status endUpdate(cock::abi::fs::Status status);

} // namespace cock::ext

#endif // !EXT_STATE_HPP
