/** @file
 * @brief The reply message of the block server.
 */
#ifndef ATA_REPLY_HPP
#define ATA_REPLY_HPP

#include <cock/abi/block.hpp>
#include <stdint.h>

namespace cock::ata {

/** @brief Value of a reply field the request does not use. */
constexpr uint32_t NO_VALUE = 0;

/**
 * @brief Sends `BLK_REPLY` to a client.
 *
 * @param pid Process id of the client.
 * @param id Request id, copied from the request.
 * @param status Result of the request.
 * @param first First value, in the third argument of the message.
 * @param second Second value, in the fourth argument of the message.
 * @param third Third value, in the fifth argument of the message.
 * @note The send result is not checked: a client that died does not need an answer.
 */
void reply(uint32_t pid, uint32_t id, cock::abi::block::Status status,
		   uint32_t first = NO_VALUE, uint32_t second = NO_VALUE, uint32_t third = NO_VALUE);

} // namespace cock::ata

#endif // !ATA_REPLY_HPP
