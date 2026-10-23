/** @file
 * @brief Mounting the volume: connecting to the block server and checking the boot sector.
 */
#ifndef FAT_MOUNT_HPP
#define FAT_MOUNT_HPP

#include <stdint.h>

namespace cock::fat {

/**
 * @brief Mounts the FAT32 volume of a block device.
 *
 * Routes the messages that arrive while the block and clock servers are awaited to
 * `Server::defer`, connects to the block server, parses the boot sector and reads the FSInfo
 * sector. The volume is served read only when the device is the whole disk or the clean
 * shutdown bit is clear. Also connects to the clock server; without it new entries get the
 * 1980 epoch.
 *
 * @param candidate Block device number, 0 for the whole disk and then the partitions.
 * @return True when the device exists and holds a valid FAT32 volume, false otherwise.
 */
bool mountVolume(uint32_t candidate);

} // namespace cock::fat

#endif // !FAT_MOUNT_HPP
