/** @file
 * @brief Allocating and freeing blocks and inodes through the bitmaps of the groups.
 */
#ifndef EXT_ALLOC_HPP
#define EXT_ALLOC_HPP

#include <cock/abi/fs.hpp>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Adds to the free counters of a group descriptor and writes it.
 *
 * @param group Group number.
 * @param blocks Change of the free block count.
 * @param inodes Change of the free inode count.
 * @param directories Change of the used directory count.
 * @return True on success, false on an I/O error.
 */
bool adjustGroup(uint32_t group, int32_t blocks, int32_t inodes, int32_t directories = 0);

/**
 * @brief Takes a free block, starting at a preferred group and wrapping around.
 *
 * Marks it in the bitmap, updates the group and the superblock counters and optionally
 * zeroes it.
 *
 * @param preferred First group to try.
 * @param zero True to write zeroes over the block.
 * @param block Receives the block number.
 * @return `OK`, `NO_SPACE` when no block is free, or `IO_ERROR`.
 */
cock::abi::fs::Status allocateBlock(uint32_t preferred, bool zero, uint32_t &block);

/**
 * @brief Returns a block to the free pool.
 *
 * @param block The block number.
 * @return True on success, false for a block outside the volume, one that was already free,
 * or an I/O error.
 */
bool freeBlock(uint32_t block);

/**
 * @brief Takes a free inode, starting at a preferred group and wrapping around.
 *
 * @param preferred First group to try.
 * @param number Receives the inode number, starting at one.
 * @return `OK`, `NO_SPACE` when no inode is free, or `IO_ERROR`.
 */
cock::abi::fs::Status allocateInode(uint32_t preferred, uint32_t &number);

/**
 * @brief Returns an inode to the free pool.
 *
 * @param number The inode number.
 * @return True on success, false for an inode that was already free or an I/O error.
 */
bool freeInode(uint32_t number);

} // namespace cock::ext

#endif // !EXT_ALLOC_HPP
