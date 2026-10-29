/** @file
 * @brief The clean shutdown bit of the volume, cleared while an operation changes it.
 */
#ifndef FAT_STATE_HPP
#define FAT_STATE_HPP

#include <cock/abi/fs.hpp>

namespace cock::fat {

/**
 * @brief Tells whether the volume was left clean by its last user.
 *
 * @return True when the clean shutdown bit is set, false when it is clear or cannot be read.
 */
bool isCleanVolume();

/**
 * @brief Clears the clean shutdown bit before a change.
 *
 * @return True on success, false on an I/O error.
 */
bool beginUpdate();

/**
 * @brief Sets the clean shutdown bit after a change.
 *
 * @param status The status of the change.
 * @return `status`, or `IO_ERROR` when the bit cannot be set.
 */
cock::abi::fs::Status endUpdate(cock::abi::fs::Status status);

} // namespace cock::fat

#endif // !FAT_STATE_HPP
