/** @file
 * @brief Waiting for the capabilities `init` grants to the block server.
 */
#ifndef ATA_GRANT_HPP
#define ATA_GRANT_HPP

namespace cock::ata {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * Every other message received meanwhile is kept with `keepPending`, to be handled by
 * `handlePending` once the drive is open.
 */
void waitForCapabilities();

} // namespace cock::ata

#endif // !ATA_GRANT_HPP
