/** @file
 * @brief The FSInfo sector: the count of free clusters and the hint for the next allocation.
 */
#ifndef FAT_FSINFO_HPP
#define FAT_FSINFO_HPP

#include <cock/abi/fat32.hpp>
#include <stdint.h>

namespace cock::fat {

/** @brief Buffer for the FSInfo sector. */
extern uint8_t fsInfoBuffer[cock::abi::fat32::SECTOR_BYTES];

/** @brief True when the volume has a valid FSInfo sector. */
extern bool fsInfoValid;

/** @brief True when the counters changed since they were last written. */
extern bool fsInfoDirty;

/** @brief True when `freeCount` is known. */
extern bool freeKnown;

/** @brief Number of free clusters, meaningful while `freeKnown` is true. */
extern uint32_t freeCount;

/** @brief Cluster where the next allocation starts looking. */
extern uint32_t nextHint;

/**
 * @brief Reads the FSInfo sector and keeps its counters.
 *
 * A volume with no valid FSInfo sector starts with no known free count and the hint at the
 * first data cluster. A hint that is not a data cluster is dropped.
 *
 * @note Call it once, after the boot sector is parsed.
 */
void loadFsInfo();

/**
 * @brief Writes the counters back when they changed.
 *
 * Does nothing without a valid FSInfo sector or when nothing changed; a failed write is not
 * reported because the counters are only hints.
 */
void flushFsInfo();

} // namespace cock::fat

#endif // !FAT_FSINFO_HPP
