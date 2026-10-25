/** @file
 * @brief The data of a file: reading and writing bytes through its block map.
 */
#ifndef EXT_FILE_HPP
#define EXT_FILE_HPP

#include <cock/abi/fs.hpp>
#include <ext/volume.hpp>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Reads bytes of a file.
 *
 * Holes read as zeros. Whole sectors are read straight into the destination and the ends
 * through `dataBuffer`.
 *
 * @param node The file or symbolic link.
 * @param destination Receives `count` bytes.
 * @param offset First byte to read; the caller has clamped it to the size of the file.
 * @param count Number of bytes; the caller has clamped it to the size of the file.
 * @return True on success, false when a pointer is outside the volume or a sector cannot be
 * read.
 */
bool readBytes(const Node &node, uint8_t *destination, uint32_t offset, uint32_t count);

/**
 * @brief Writes bytes of a file, allocating blocks as needed.
 *
 * Writes the inode when the size or the sector count changed. A write that stored some bytes
 * before the volume filled up reports `OK` with the short count in `written`.
 *
 * @param node The file.
 * @param source The bytes to write.
 * @param offset First byte to write.
 * @param count Number of bytes.
 * @param written Receives the number of bytes stored.
 * @return `OK`, `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status writeBlocks(Node &node, const uint8_t *source, uint32_t offset,
								  uint32_t count, uint32_t &written);

} // namespace cock::ext

#endif // !EXT_FILE_HPP
