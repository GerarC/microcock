/** @file
 * @brief The reply message of the `vfs` server.
 */
#ifndef VFS_REPLY_HPP
#define VFS_REPLY_HPP

#include <cock/abi/fs.hpp>
#include <stdint.h>

namespace cock::vfs {

/** @brief Value of a reply field the request does not use. */
constexpr uint32_t NO_VALUE = 0;

/**
 * @brief Sends `FS_REPLY` to a client.
 *
 * @param pid Process id of the client.
 * @param id Request id, copied from the request.
 * @param status Result of the request.
 * @param first First value, in the third argument of the message.
 * @param second Second value, in the fourth argument of the message.
 * @note The send result is not checked: a client that died does not need an answer.
 */
void reply(uint32_t pid, uint32_t id, cock::abi::fs::Status status, uint32_t first = NO_VALUE,
		   uint32_t second = NO_VALUE);

} // namespace cock::vfs

#endif // !VFS_REPLY_HPP
