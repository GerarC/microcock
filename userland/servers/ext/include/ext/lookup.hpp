/** @file
 * @brief Resolving paths, following symbolic links, and splitting a path into parent and name.
 */
#ifndef EXT_LOOKUP_HPP
#define EXT_LOOKUP_HPP

#include <ext/volume.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Resolves a path from the root, following symbolic links.
 *
 * Follows at most `MAX_LINKS` links per path; an absolute target restarts at the root of the
 * volume.
 *
 * @param path Path, terminated by zero; the empty path is the root.
 * @param result Receives the node the path names.
 * @return True when every component exists, false for a missing component, a file used as a
 * directory, a path or name that is too long, too many links, or an I/O error.
 */
bool resolve(const char *path, Node &result);

/**
 * @brief Splits a path into its parent directory and its last name.
 *
 * @param path Path, terminated by zero.
 * @param parent Receives the parent directory, links followed.
 * @param name Receives the last name, terminated by zero; it must hold `MAX_NAME_BYTES + 1`.
 * @param nameBytes Receives the length of the name.
 * @return True on success, false when the path has no name, a name is too long, or the parent
 * does not exist or is not a directory.
 */
bool parentAndName(const char *path, Node &parent, char *name, size_t &nameBytes);

/**
 * @brief Resolves a path without following a link in its last component.
 *
 * @param path Path, terminated by zero.
 * @param node Receives the node.
 * @return True when the path exists, false otherwise.
 */
bool resolveLeaf(const char *path, Node &node);

/**
 * @brief Tells whether a directory is an ancestor of another one, or the same.
 *
 * Climbs the `..` entries up to the root. A directory with no readable `..` and a climb longer
 * than `MAX_DEPTH` count as a yes, so a damaged tree never allows a move.
 *
 * @param directory Candidate ancestor, an inode number.
 * @param start Inode number of the directory to climb from.
 * @return True when `directory` is `start` or above it, or when the answer cannot be known.
 */
bool isAncestor(uint32_t directory, uint32_t start);

} // namespace cock::ext

#endif // !EXT_LOOKUP_HPP
