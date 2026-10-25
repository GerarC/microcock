/** @file
 * @brief The data of a file: reading, writing and truncating through its cluster chain.
 */
#ifndef FAT_FILE_HPP
#define FAT_FILE_HPP

#include <cock/abi/fs.hpp>
#include <fat/volume.hpp>
#include <stdint.h>

namespace cock::fat {

/**
 * @brief Reads bytes of a file.
 *
 * Reads whole sectors straight into the destination and the ends through `dataBuffer`. Remembers
 * the last cluster reached in the handle so sequential reads do not walk the chain again.
 *
 * @param handle The open file.
 * @param destination Receives `count` bytes.
 * @param offset First byte to read; the caller has clamped it to the size of the file.
 * @param count Number of bytes; the caller has clamped it to the size of the file.
 * @return True on success, false when the chain is not valid or a sector cannot be read.
 */
bool readBytes(Handle &handle, uint8_t *destination, uint32_t offset, uint32_t count);

/**
 * @brief Writes bytes of a file, growing its chain.
 *
 * Writes the directory entry when the size or the first cluster changed, and flushes the
 * FSInfo counters. A write that stored some bytes before the volume filled up reports `OK`
 * with the short count in `written`.
 *
 * @param handle The open file.
 * @param source The bytes to write.
 * @param offset First byte to write.
 * @param count Number of bytes.
 * @param written Receives the number of bytes stored.
 * @return `OK`, `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status writeBytes(Handle &handle, const uint8_t *source, uint32_t offset,
								 uint32_t count, uint32_t &written);

/**
 * @brief Cuts a file to zero length and frees its clusters.
 *
 * The directory entry is written before the chain is freed, so a crash leaves lost clusters
 * and never an entry that points at free ones. On failure the node keeps its old size.
 *
 * @param node The file.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status truncateNode(Node &node);

} // namespace cock::fat

#endif // !FAT_FILE_HPP
