/** @file
 * @brief One handler for each kind of request a client sends to the block server.
 *
 * Every handler answers the client with `BLK_REPLY`, success or not; none returns a value.
 * Requests from a process without a session never reach them: `handle` refuses them.
 */
#ifndef ATA_REQUESTS_HPP
#define ATA_REQUESTS_HPP

#include <ata/sessions.hpp>
#include <cock/ipc/message.hpp>

namespace cock::ata {

/**
 * @brief Handles one message: `BLK_CONNECT`, or a request from a connected client.
 *
 * Messages from the kernel and message types that are not block requests are ignored. A
 * request from a process without a session gets `NOT_CONNECTED`.
 *
 * @param message Message received, never trusted.
 */
void handle(const cock::ipc::Message &message);

/**
 * @brief Handles `BLK_INFO`: size, first sector and kind of a device.
 *
 * @param message Request: device number in the second argument.
 * @note Replies `BAD_DEVICE` for a number at or above the device count.
 */
void answerInfo(const cock::ipc::Message &message);

/**
 * @brief Handles `BLK_READ`: reads sectors of a device into the window of the client.
 *
 * @param session Session of the sender.
 * @param message Request: device, first sector and sector count in the second to fourth
 * arguments.
 * @note Replies `BAD_DEVICE`, `BAD_REQUEST` for a count of zero or above the limit,
 * `OUT_OF_RANGE` for a range past the end of the device and `IO_ERROR` when the drive fails;
 * the value is the sector count.
 */
void answerRead(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `BLK_WRITE`: writes sectors of a device from the window of the client.
 *
 * @param session Session of the sender.
 * @param message Request: device, first sector and sector count in the second to fourth
 * arguments.
 * @note Replies `READ_ONLY` for the whole disk, which is never written, and otherwise the
 * same statuses as a read.
 */
void answerWrite(Session &session, const cock::ipc::Message &message);

} // namespace cock::ata

#endif // !ATA_REQUESTS_HPP
