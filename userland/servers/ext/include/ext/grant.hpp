/** @file
 * @brief Waiting for the grant `rs` sends to the ext2 server.
 */
#ifndef EXT_GRANT_HPP
#define EXT_GRANT_HPP

#include <stdint.h>

namespace cock::ext {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is queued with `Server::defer`, to be served once
 * the server runs.
 *
 * @return The block device to serve, from the second argument of the grant message.
 */
uint32_t waitForCapabilities();

} // namespace cock::ext

#endif // !EXT_GRANT_HPP
