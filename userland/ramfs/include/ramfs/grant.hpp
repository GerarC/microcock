/** @file
 * @brief Waiting for the capabilities `rs` grants to the `ramfs` server.
 */
#ifndef RAMFS_GRANT_HPP
#define RAMFS_GRANT_HPP

namespace cock::ramfs {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is kept with `keepPending`, to be handled by
 * `handlePending` once the archive is loaded.
 */
void waitForCapabilities();

} // namespace cock::ramfs

#endif // !RAMFS_GRANT_HPP
