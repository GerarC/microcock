/** @file
 * @brief The clock hardware and the one request the `rtc` server answers.
 */
#ifndef RTC_REQUESTS_HPP
#define RTC_REQUESTS_HPP

#include <cock/ipc/message.hpp>

namespace cock::rtc {

/**
 * @brief Opens the CMOS clock registers.
 *
 * Needs the port capability the server receives from `rs`, so it must run after
 * `CAPABILITY_GRANTED`.
 *
 * @return True when the registers can be read, false when the ports are not granted.
 */
bool initClock();

/**
 * @brief Handles one message: answers `RTC_READ` with the current date and time.
 *
 * A message that carries no reply token (the kernel's notices, and anything sent by process id) and
 * message types other than `RTC_READ` are ignored: the server answers only through a token.
 *
 * @param message Message received, never trusted.
 * @note Replies `UNAVAILABLE` when the clock cannot be read.
 */
void handle(const cock::ipc::Message &message);

} // namespace cock::rtc

#endif // !RTC_REQUESTS_HPP
