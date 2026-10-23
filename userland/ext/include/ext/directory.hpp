/** @file
 * @brief Reading and changing the records of a directory.
 */
#ifndef EXT_DIRECTORY_HPP
#define EXT_DIRECTORY_HPP

#include <cock/abi/ext2.hpp>
#include <cock/abi/fs.hpp>
#include <ext/volume.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Loads a whole block of a directory into `directoryBuffer`.
 *
 * @param physical Block number on the volume.
 * @return True when the block is loaded, false on an I/O error.
 */
bool loadDirectoryBlock(uint32_t physical);

/**
 * @brief Writes `directoryBuffer` back to a block.
 *
 * @param physical Block number on the volume.
 * @return True on success. On failure the buffer is forgotten.
 */
bool storeDirectoryBlock(uint32_t physical);

/**
 * @brief Reads the next live record of a directory.
 *
 * Skips holes, deleted records and records with no name; a block whose records do not parse
 * is skipped whole.
 *
 * @param directory The directory.
 * @param offset Byte offset in the directory, advanced past the record.
 * @param item Receives the record, pointing into `directoryBuffer`.
 * @return True when a record was read, false at the end or on an I/O error.
 */
bool nextEntry(const Node &directory, uint32_t &offset, cock::abi::ext2::DirItem &item);

/**
 * @brief Looks a name up in one directory.
 *
 * @param directory The directory.
 * @param name Name to find, terminated by zero; the comparison is exact.
 * @param inode Receives the inode number.
 * @return True when found, false when absent or the directory cannot be read.
 */
bool findEntry(const Node &directory, const char *name, uint32_t &inode);

/**
 * @brief Finds where a name is recorded, so it can be removed.
 *
 * @param directory The directory.
 * @param name Name to find, terminated by zero.
 * @param where Receives the block, offset and neighbour of the record.
 * @return True when found, false when absent or the directory cannot be read.
 */
bool locateEntry(const Node &directory, const char *name, Where &where);

/**
 * @brief Adds a record to a directory.
 *
 * Reuses the slack of an existing record when the new one fits and otherwise grows the
 * directory by one block.
 *
 * @param directory The directory; its size and sectors change when it grows.
 * @param name The name.
 * @param nameBytes Length of the name.
 * @param inode Inode the record refers to.
 * @param type `DIRENT_TYPE_*` value stored in the record.
 * @return `OK`, `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status addEntry(Node &directory, const char *name, uint32_t nameBytes,
							   uint32_t inode, uint8_t type);

/**
 * @brief Removes a record found with `locateEntry`.
 *
 * The record is merged into the one before it, or its inode is set to zero when it is first.
 *
 * @param where The place of the record.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status removeEntry(const Where &where);

/**
 * @brief Tells whether a directory holds nothing but `.` and `..`.
 *
 * @param node The directory.
 * @return True when it is empty.
 */
bool directoryIsEmpty(const Node &node);

/**
 * @brief Gives the name and type of the n-th entry of a directory.
 *
 * Skips `.` and `..`, names that do not fit and entries whose inode cannot be read.
 *
 * @param directory The directory.
 * @param wanted Index of the entry among the visible ones.
 * @param name Receives the name, terminated by zero.
 * @param capacity Size of `name` in bytes.
 * @param type Receives the type of the entry.
 * @return True on success, false when there is no such entry.
 */
bool listEntry(const Node &directory, uint32_t wanted, char *name, size_t capacity,
			   cock::abi::fs::FileType &type);

} // namespace cock::ext

#endif // !EXT_DIRECTORY_HPP
