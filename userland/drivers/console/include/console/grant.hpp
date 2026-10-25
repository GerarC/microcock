/** @file
 * @brief Waiting for the capabilities `rs` grants to the console server.
 */
#ifndef CONSOLE_GRANT_HPP
#define CONSOLE_GRANT_HPP

namespace cock::console {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is dropped.
 */
void waitForCapabilities();

} // namespace cock::console

#endif // !CONSOLE_GRANT_HPP
