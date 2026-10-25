/** @file
 * @brief Queue of requests that arrive while `vfs` waits for a backend server.
 */
#ifndef VFS_PENDING_HPP
#define VFS_PENDING_HPP

#include <cock/ipc/message.hpp>

namespace cock::vfs {

/**
 * @brief Keeps a message that arrived while a reply was awaited, to handle it later.
 *
 * It is the `onForeign` hook of every backend client.
 *
 * @param message Message to keep, copied.
 * @note The queue holds a fixed number of messages; when it is full the message is dropped.
 */
void defer(const cock::ipc::Message &message);

/**
 * @brief Takes the oldest deferred message out of the queue.
 *
 * @param message Receives the message when there is one; untouched otherwise.
 * @return True when a message was taken, false when the queue is empty.
 */
bool takePending(cock::ipc::Message &message);

} // namespace cock::vfs

#endif // !VFS_PENDING_HPP
