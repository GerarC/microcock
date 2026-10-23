/** @file
 * @brief Waiting for the capabilities `init` grants to the keyboard driver.
 */
#ifndef KBD_GRANT_HPP
#define KBD_GRANT_HPP

#include <stdint.h>

namespace cock::kbd {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is dropped.
 *
 * @return The process id the message carries in its first argument: the console server that
 * receives the keys.
 */
uint32_t waitForCapabilities();

} // namespace cock::kbd

#endif // !KBD_GRANT_HPP
