/** @file
 * @brief The block map of an inode: direct and indirect blocks, mapping, growing and freeing.
 */
#ifndef EXT_BLOCKMAP_HPP
#define EXT_BLOCKMAP_HPP

#include <cock/abi/fs.hpp>
#include <ext/volume.hpp>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Reads one pointer of a block of pointers.
 *
 * @param block The block of pointers; zero reads as a zero pointer.
 * @param index Index of the pointer in the block.
 * @param value Receives the pointer.
 * @return True on success, false for a block outside the volume or an I/O error.
 */
bool readPointer(uint32_t block, uint32_t index, uint32_t &value);

/**
 * @brief Finds the block of a file that holds a given file block.
 *
 * @param node The file.
 * @param fileBlock Index of the block inside the file.
 * @param physical Receives the block number on the volume, zero for a hole.
 * @return True on success, false for an index past the largest file, a pointer outside the
 * volume, or an I/O error.
 */
bool physicalBlock(const Node &node, uint32_t fileBlock, uint32_t &physical);

/**
 * @brief Finds the block of a file for writing, allocating it and its blocks of indirection.
 *
 * Adds the new blocks to `node.sectors`; the caller writes the inode.
 *
 * @param node The file.
 * @param fileBlock Index of the block inside the file.
 * @param zeroNew True to zero a new data block; blocks of indirection are always zeroed.
 * @param physical Receives the block number on the volume.
 * @return `OK`, `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status mapForWrite(Node &node, uint32_t fileBlock, bool zeroNew,
								  uint32_t &physical);

/**
 * @brief Frees every block a block map refers to, indirect blocks included.
 *
 * @param blocks The `INODE_BLOCK_SLOTS` slots of an inode.
 * @return True when every block was freed, false when some could not be.
 */
bool releaseBlocks(const uint32_t *blocks);

/**
 * @brief Empties a file: writes the inode with no blocks and then frees them.
 *
 * The inode is written first, so a crash leaves unreferenced blocks and never an inode that
 * points at free ones. On failure to write it the node keeps its old fields.
 *
 * @param node The file.
 * @param deleted True to stamp the deletion time.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status emptyNode(Node &node, bool deleted = false);

} // namespace cock::ext

#endif // !EXT_BLOCKMAP_HPP
