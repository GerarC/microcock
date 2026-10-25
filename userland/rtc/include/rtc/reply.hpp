/** @file
 * @brief The reply message of the `rtc` server.
 */
#ifndef RTC_REPLY_HPP
#define RTC_REPLY_HPP

#include <cock/abi/rtc.hpp>
#include <stdint.h>

namespace cock::rtc {

/** @brief Value of a reply field the request does not use. */
constexpr uint32_t NO_VALUE = 0;

/**
 * @brief Sends `RTC_REPLY` to the client that asked, through the reply token of its request.
 *
 * @param token `Message::reply` of the request; the answer goes to the thread that sent it.
 * @param id Request id, copied from the request.
 * @param status Result of the request.
 * @param date Packed date, in the third argument of the message.
 * @param time Packed time, in the fourth argument of the message.
 * @note The send result is not checked: a client that died does not need an answer.
 */
void reply(uint32_t token, uint32_t id, cock::abi::rtc::Status status, uint32_t date = NO_VALUE,
		   uint32_t time = NO_VALUE);

} // namespace cock::rtc

#endif // !RTC_REPLY_HPP
