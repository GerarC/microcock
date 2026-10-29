/** @file
 * @brief The drive and the table of block devices: the whole disk and its partitions.
 */
#ifndef ATA_DEVICES_HPP
#define ATA_DEVICES_HPP

#include <ata/drive.hpp>
#include <cock/abi/block.hpp>
#include <stdint.h>

namespace cock::ata {

/**
 * @brief A block device clients can address: the whole disk or one partition.
 */
struct Device {
	uint32_t firstSector; /**< First sector of the device on the disk. */
	uint32_t sectorCount; /**< Size of the device in sectors. */
	cock::abi::block::DeviceKind kind; /**< Whole disk, MBR partition or GPT partition. */
};

/** @brief The master drive of the primary channel. Usable after `Drive::init` succeeds. */
extern Drive drive;

/** @brief The devices, the whole disk first. Only the first `deviceCount` are valid. */
extern Device devices[cock::abi::block::MAX_DEVICES];

/** @brief Number of valid entries in `devices`. */
extern uint32_t deviceCount;

/**
 * @brief Adds the whole disk and then every partition found in its partition table.
 *
 * Reads the MBR, and the GPT when the MBR is a protective one. A table that cannot be read
 * or is rejected leaves only the whole disk; a rejected GPT prints `ata: GPT rejected`.
 *
 * @note Call it once, after the drive is open.
 */
void scanPartitions();

} // namespace cock::ata

#endif // !ATA_DEVICES_HPP
