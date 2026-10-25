/** @file
 * @brief The directory `ls` lists: the word typed against the working directory.
 */
#ifndef LS_LOCATION_HPP
#define LS_LOCATION_HPP

#include <cock/abi/fs.hpp>
#include <stddef.h>

namespace cock::ls {

/** @brief Size in bytes of a buffer that holds an absolute path, the terminator included. */
constexpr size_t PATH_BYTES = cock::abi::fs::MAX_PATH_BYTES;

/**
 * @brief Resolves a path against the working directory (`getcwd`).
 *
 * The file servers see absolute paths only.
 *
 * @param path The word as typed: absolute or relative, with `.` and `..` allowed.
 * @param out Receives the absolute, normalized path.
 * @return True on success; false when the result does not fit, with `errno` set to `ENAMETOOLONG`.
 */
bool resolveLocation(const char *path, char (&out)[PATH_BYTES]);

} // namespace cock::ls

#endif // !LS_LOCATION_HPP
