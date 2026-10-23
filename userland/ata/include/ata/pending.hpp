/** @file
 * @brief Requests that arrive before the server has its capability and can use the drive.
 */
#ifndef ATA_PENDING_HPP
#define ATA_PENDING_HPP

#include <cock/ipc/message.hpp>

namespace cock::ata {

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
 * Call it once, after the drive is open and the devices are scanned.
 */
void handlePending();

} // namespace cock::ata

#endif // !ATA_PENDING_HPP
