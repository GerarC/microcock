/** @file
 * @brief Requests that arrive before the server has its capability and can read the archive.
 */
#ifndef RAMFS_PENDING_HPP
#define RAMFS_PENDING_HPP

#include <cock/ipc/message.hpp>

namespace cock::ramfs {

/**
 * @brief Keeps a message that arrived before `CAPABILITY_GRANTED`.
 *
 * @param message Message to keep, copied.
 * @note The list holds a fixed number of messages; when it is full the message is dropped.
 */
void keepPending(const cock::ipc::Message &message);

/**
 * @brief Handles every kept message in the order it arrived, then empties the list.
 *
 * Call it once, after the archive is loaded.
 */
void handlePending();

} // namespace cock::ramfs

#endif // !RAMFS_PENDING_HPP
