/** @file
 * @brief Resolving paths to the files and directories of the volume.
 */
#ifndef FAT_LOOKUP_HPP
#define FAT_LOOKUP_HPP

#include <cock/abi/fs.hpp>
#include <fat/volume.hpp>
#include <stddef.h>

namespace cock::fat {

/**
 * @brief Walks a path from the root directory.
 *
 * Repeated separators are skipped. A component longer than a name, a file used as a directory
 * and a cluster number that is not a data cluster all fail the walk.
 *
 * @param path Path, terminated by zero; the empty path is the root.
 * @param node Receives the file or directory the path names.
 * @param leaf Receives the last entry found; unchanged for the root.
 * @return True when every component exists, false otherwise.
 */
bool walk(const char *path, Node &node, Found &leaf);

/**
 * @brief Resolves a path to a node.
 *
 * @param path Path, terminated by zero.
 * @param node Receives the node.
 * @return True when the path exists, false otherwise.
 */
bool resolve(const char *path, Node &node);

/**
 * @brief Splits a path into its parent directory and its last name.
 *
 * @param path Path, terminated by zero.
 * @param parent Receives the parent directory.
 * @param name Receives the last name, terminated by zero; it must hold `MAX_PATH_BYTES`.
 * @param nameBytes Receives the length of the name.
 * @return `OK`, `BAD_REQUEST` when the path has no name or is too long, or `NOT_FOUND` when
 * the parent does not exist or is not a directory.
 */
cock::abi::fs::Status parentAndName(const char *path, Node &parent, char *name,
									size_t &nameBytes);

/**
 * @brief Tells whether a path lies inside another one, or is the same.
 *
 * Compares component by component, ignoring ASCII case.
 *
 * @param ancestor Candidate ancestor, terminated by zero.
 * @param path Path to test, terminated by zero.
 * @return True when every component of `ancestor` starts `path`.
 */
bool isInside(const char *ancestor, const char *path);

} // namespace cock::fat

#endif // !FAT_LOOKUP_HPP
