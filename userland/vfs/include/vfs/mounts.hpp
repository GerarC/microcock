/** @file
 * @brief The mount table: which server owns which path prefix, and the link to each server.
 */
#ifndef VFS_MOUNTS_HPP
#define VFS_MOUNTS_HPP

#include <cock/abi/fs.hpp>
#include <cock/abi/mounts.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>

namespace cock::vfs {

/**
 * @brief One line of the mount table with the connection to its server.
 */
struct Mount {
	char prefix[cock::abi::mounts::MAX_PREFIX_BYTES]; /**< Path prefix the server owns. */
	char server[cock::abi::mounts::MAX_SERVER_BYTES]; /**< Name of the server process. */
	size_t prefixLength; /**< Length of `prefix` in bytes. */
	cock::user::fs::Client client; /**< The `vfs` as a client of that server. */
	bool connected; /**< True while the connection is believed to be alive. */
};

/** @brief The mounts, in the order of `mounts.conf`. Only the first `mountCount` are valid. */
extern Mount mounts[cock::abi::mounts::MAX_MOUNTS];

/** @brief Number of valid entries in `mounts`. */
extern size_t mountCount;

/**
 * @brief Reads `mounts.conf` from `ramfs` and fills the mount table.
 *
 * Prints the line and the error when the file is rejected.
 *
 * @return True when the table was loaded, false when `ramfs` is unreachable, the file is
 * missing, empty, too large, unreadable or rejected by the parser.
 */
bool loadMounts();

/**
 * @brief Finds the mount that owns an absolute path.
 *
 * The longest prefix that ends at a component boundary wins, so `/fatx` belongs to `/` and
 * not to `/fat`.
 *
 * @param full Absolute path, terminated by zero.
 * @param rest Receives the path inside the mount: `full` without the prefix, or the root
 * prefix when nothing is left.
 * @return Index into `mounts`, or `NOT_FOUND_MOUNT` when no prefix matches.
 */
size_t pickMount(const char *full, const char *&rest);

/**
 * @brief Connects to the server of a mount the first time it is used.
 *
 * @param index Index into `mounts`, below `mountCount`.
 * @return True when the server is connected, false when it is not running.
 */
bool ensureMount(size_t index);

/**
 * @brief Marks a mount as down and forgets every open file that belongs to it.
 *
 * @param index Index into `mounts`, below `mountCount`.
 */
void markDown(size_t index);

/**
 * @brief Turns the last failure of a backend client into the status for the caller.
 *
 * A `NOT_CONNECTED` status marks the mount as down. A status that is `OK` after a failed
 * call becomes `IO_ERROR`.
 *
 * @param index Index into `mounts` of the server that failed.
 * @return The status to send back to the client.
 */
cock::abi::fs::Status failure(size_t index);

/**
 * @brief Tells whether a mount shows up as a directory in the listing of `/`.
 *
 * Only prefixes with a single component do, such as `/fat`.
 *
 * @param mount Mount to test.
 * @param name Receives the name of the directory, without the leading separator, when the
 * result is true.
 * @return True for a single component prefix, false otherwise.
 */
bool isMountPoint(const Mount &mount, const char *&name);

} // namespace cock::vfs

#endif // !VFS_MOUNTS_HPP
