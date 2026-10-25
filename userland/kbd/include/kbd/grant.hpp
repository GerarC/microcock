/** @file
 * @brief Waiting for the capabilities `rs` grants to the keyboard driver.
 */
#ifndef KBD_GRANT_HPP
#define KBD_GRANT_HPP

namespace cock::kbd {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is dropped.
 */
void waitForCapabilities();

} // namespace cock::kbd

#endif // !KBD_GRANT_HPP
