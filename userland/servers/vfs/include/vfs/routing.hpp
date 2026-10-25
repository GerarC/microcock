/** @file
 * @brief Turns the path in a client window into a mount and a path inside that mount.
 */
#ifndef VFS_ROUTING_HPP
#define VFS_ROUTING_HPP

#include <cock/abi/fs.hpp>
#include <vfs/clients.hpp>
#include <vfs/limits.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::vfs {

/** @brief Size of the buffer that holds an absolute path and its terminator. */
constexpr size_t FULL_PATH_BYTES = cock::abi::fs::MAX_PATH_BYTES + SEPARATOR_BYTES;

/**
 * @brief Reads a path from the window of a client and finds the server that owns it.
 *
 * The path is made absolute, checked, matched against the mount table and the server of the
 * mount is connected if needed.
 *
 * @param session Session of the client, whose window holds the path.
 * @param at Byte offset of the path inside the window.
 * @param length Length of the path in bytes, taken from the request.
 * @param full Receives the absolute path; must hold `FULL_PATH_BYTES` bytes.
 * @param index Receives the index into `mounts` of the owner.
 * @param rest Receives the path inside the mount; it points into `full` or at the root prefix.
 * @return `OK` on success, `BAD_REQUEST` for a path that is empty, too long, has an embedded
 * zero or matches no mount, and `NOT_FOUND` when the server of the mount is not running.
 */
cock::abi::fs::Status route(const Session &session, size_t at, uint32_t length, char *full,
							size_t &index, const char *&rest);

} // namespace cock::vfs

#endif // !VFS_ROUTING_HPP
