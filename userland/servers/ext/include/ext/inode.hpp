/** @file
 * @brief Group descriptors and inodes: reading them and writing the inode back.
 */
#ifndef EXT_INODE_HPP
#define EXT_INODE_HPP

#include <ext/volume.hpp>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Loads the sector of the group descriptor table that holds a group.
 *
 * @param group Group number.
 * @param offset Receives the offset of the descriptor inside `groupBuffer`.
 * @return True on success, false for a group past the end or an I/O error.
 */
bool loadGroup(uint32_t group, uint32_t &offset);

/**
 * @brief Finds the sector of the inode table that holds an inode.
 *
 * Checks that the inode table lies inside the volume.
 *
 * @param number Inode number, starting at one.
 * @param sector Receives the sector on the device.
 * @param within Receives the offset of the inode inside the sector.
 * @return True on success, false for an invalid number, a bad table or an I/O error.
 */
bool inodeLocation(uint32_t number, uint32_t &sector, uint32_t &within);

/**
 * @brief Reads an inode.
 *
 * Refuses inodes that use extents or inline data, and types other than regular files,
 * directories and symbolic links.
 *
 * @param number Inode number, starting at one.
 * @param node Receives the inode.
 * @return True on success, false otherwise.
 */
bool loadInode(uint32_t number, Node &node);

/**
 * @brief Writes an inode back with the current time as its change and modify time.
 *
 * @param node The inode, with its size, links, sectors and block map.
 * @param newMode `NOT_NEW` to keep the other fields of the inode, or the mode of a new inode,
 * which clears the inode first.
 * @param deleted True to stamp the deletion time, as `unlink` does.
 * @return True on success, false on an I/O error.
 */
bool storeInode(const Node &node, uint32_t newMode = NOT_NEW, bool deleted = false);

} // namespace cock::ext

#endif // !EXT_INODE_HPP
