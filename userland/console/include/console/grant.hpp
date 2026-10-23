/** @file
 * @brief Waiting for the capabilities `init` grants to the console server.
 */
#ifndef CONSOLE_GRANT_HPP
#define CONSOLE_GRANT_HPP

#include <stdint.h>

namespace cock::console {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is dropped.
 *
 * @return The process id the message carries in its first argument: the display server.
 */
uint32_t waitForCapabilities();

} // namespace cock::console

#endif // !CONSOLE_GRANT_HPP
