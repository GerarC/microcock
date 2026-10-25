/** @file
 * @brief Waiting for the capabilities `rs` grants to the clock server.
 */
#ifndef RTC_GRANT_HPP
#define RTC_GRANT_HPP

namespace cock::rtc {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is kept with `keepPending`, to be handled by
 * `handlePending` once the clock is open.
 */
void waitForCapabilities();

} // namespace cock::rtc

#endif // !RTC_GRANT_HPP
