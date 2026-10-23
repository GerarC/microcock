/** @file
 * @brief The file allocation table: following, allocating and freeing cluster chains.
 */
#ifndef FAT_TABLE_HPP
#define FAT_TABLE_HPP

#include <cock/abi/fs.hpp>
#include <stdint.h>

#include <fat/volume.hpp>

namespace cock::fat {

/**
 * @brief Gives the sector of the first FAT that holds the link of a cluster.
 *
 * @param cluster The cluster.
 * @return Sector number on the device.
 */
uint32_t fatSector(uint32_t cluster);

/**
 * @brief Loads a sector of the first FAT into `fatBuffer`.
 *
 * @param sector Sector number on the device.
 * @return True when the sector is loaded, false when it cannot be read.
 */
bool loadFat(uint32_t sector);

/**
 * @brief Writes `fatBuffer` to every copy of the FAT.
 *
 * @return True when all copies are written. On failure the buffer is forgotten.
 */
bool storeFat();

/**
 * @brief Sets the link of a cluster and writes it.
 *
 * @param cluster The cluster.
 * @param value The new link: a cluster, the end of chain mark or free.
 * @return True on success, false on an I/O error.
 */
bool setLink(uint32_t cluster, uint32_t value);

/**
 * @brief Reads the link of a cluster.
 *
 * @param cluster The cluster.
 * @param next Receives the next cluster when the result is `Link::NEXT`.
 * @return `NEXT`, `END` for the end of the chain, or `BAD` for a value that is not a data
 * cluster or a FAT sector that cannot be read.
 */
Link nextCluster(uint32_t cluster, uint32_t &next);

/**
 * @brief Takes a free cluster, marks it as the end of a chain and links it after another.
 *
 * Starts at `nextHint` and wraps around once. Updates the free count and marks the FSInfo
 * counters dirty; the caller flushes them.
 *
 * @param previous Cluster whose link receives the new one, or `NO_CLUSTER` to start a chain.
 * @param cluster Receives the new cluster.
 * @return `OK`, `NO_SPACE` when no cluster is free, or `IO_ERROR`.
 */
cock::abi::fs::Status allocateCluster(uint32_t previous, uint32_t &cluster);

/**
 * @brief Frees every cluster of a chain.
 *
 * @param first First cluster of the chain.
 * @return True when the chain ended properly, false on an I/O error, a bad link or a chain
 * longer than the volume.
 */
bool freeChain(uint32_t first);

/**
 * @brief Writes zeroes over a whole cluster and forgets the directory buffer.
 *
 * @param cluster The cluster.
 * @return True on success, false on an I/O error.
 */
bool zeroCluster(uint32_t cluster);

} // namespace cock::fat

#endif // !FAT_TABLE_HPP
