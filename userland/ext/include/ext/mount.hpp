/** @file
 * @brief Mounting the volume: connecting to the block server and checking the superblock.
 */
#ifndef EXT_MOUNT_HPP
#define EXT_MOUNT_HPP

#include <stdint.h>

namespace cock::ext {

/**
 * @brief Mounts the ext2 volume of a block device.
 *
 * Routes the messages that arrive while the block and clock servers are awaited to
 * `Server::defer`, connects to the block server, parses and checks the superblock and reads
 * the root inode. Each refusal prints its reason: a missing device, no superblock, an
 * unsupported block or inode size, a geometry that does not fit, an unsupported feature or a
 * damaged root. A volume the features do not allow to write is mounted read only and says so.
 *
 * @param candidate Block device number, 0 for the whole disk and then the partitions.
 * @return True when the volume is mounted, false when it was refused.
 */
bool mountVolume(uint32_t candidate);

} // namespace cock::ext

#endif // !EXT_MOUNT_HPP
