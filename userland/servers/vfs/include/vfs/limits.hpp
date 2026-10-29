/** @file
 * @brief Limits and path constants shared by the modules of the `vfs` server.
 */
#ifndef VFS_LIMITS_HPP
#define VFS_LIMITS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::vfs {

/** @brief Number of client sessions, each with its own window, kept at once. */
constexpr size_t MAX_CLIENTS = 8;

/** @brief Size of the table of open files, shared by all clients. */
constexpr size_t MAX_OPEN = 16;

/** @brief Character that separates the components of a path. */
constexpr char PATH_SEPARATOR = '/';

/** @brief Value `pickMount` returns when no mount owns a path. */
constexpr size_t NOT_FOUND_MOUNT = static_cast<size_t>(-1);

/** @brief Length in bytes of one path separator. */
constexpr size_t SEPARATOR_BYTES = 1;

/** @brief Offset and counter value that stands for the first element. */
constexpr uint32_t START_OFFSET = 0;

} // namespace cock::vfs

#endif // !VFS_LIMITS_HPP
