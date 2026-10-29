/** @file
 * @brief The time stored in changed inodes and in the superblock.
 */
#ifndef EXT_STAMP_HPP
#define EXT_STAMP_HPP

#include <stdint.h>

namespace cock::ext {

/**
 * @brief Reads the current time from the clock server as seconds since 1970.
 *
 * @return The time, or the fixed mark `DELETION_TIME_MARK` when the clock server is not
 * available or does not answer.
 */
uint32_t timestamp();

} // namespace cock::ext

#endif // !EXT_STAMP_HPP
