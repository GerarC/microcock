/** @file
 * @brief The mounted ext2 volume: its superblock, the sector buffers and the node types.
 */
#ifndef EXT_VOLUME_HPP
#define EXT_VOLUME_HPP

#include <cock/abi/block.hpp>
#include <cock/abi/ext2.hpp>
#include <cock/abi/fs.hpp>
#include <cock/user/block.hpp>
#include <cock/user/clock.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::ext {

/** @brief Value of a buffer's loaded sector while the buffer holds nothing valid. */
constexpr uint32_t NOTHING_LOADED = 0xFFFFFFFF;

/** @brief Sector count of a request for one sector. */
constexpr uint32_t SINGLE_SECTOR = 1;

/** @brief Size of a device sector in bytes. */
constexpr size_t SECTOR_BYTES = cock::abi::ext2::SECTOR_BYTES;

/** @brief Longest name of a directory entry. */
constexpr size_t MAX_NAME_BYTES = 255;

/** @brief Size and byte count of an empty file. */
constexpr uint32_t NO_BYTES = 0;

/** @brief Block number that means "no block": a hole in a file. */
constexpr uint32_t NO_BLOCK = 0;

/** @brief Sector count of a file with no blocks. */
constexpr uint32_t NO_SECTORS = 0;

/** @brief Depth of the first level of indirection. */
constexpr uint32_t FIRST_LEVEL = 1;

/** @brief Mode value of `storeInode` that means "keep the inode, do not initialise it". */
constexpr uint32_t NOT_NEW = 0;

/** @brief Name of the entry that refers to a directory itself. */
constexpr char CURRENT_NAME[] = ".";

/** @brief Name of the entry that refers to the parent of a directory. */
constexpr char PARENT_NAME[] = "..";

/**
 * @brief A file, a directory or a symbolic link: the fields of its inode that the server keeps.
 */
struct Node {
	uint32_t inode; /**< Inode number, starting at one. */
	uint32_t size; /**< Size in bytes. */
	cock::abi::fs::FileType type; /**< Regular file, directory or symbolic link. */
	uint32_t links; /**< Number of directory entries that refer to it. */
	uint32_t sectors; /**< Sectors in use, counting the blocks of indirection. */
	uint32_t blocks[cock::abi::ext2::INODE_BLOCK_SLOTS]; /**< The block map of the inode. */
};

/**
 * @brief An open file of a client session.
 */
struct Handle {
	bool open; /**< True while the handle is in use. */
	Node node; /**< The file it refers to. */
};

/**
 * @brief Where a directory entry lives, so it can be removed.
 */
struct Where {
	uint32_t physical; /**< Block of the directory that holds the record. */
	uint32_t offset; /**< Offset of the record inside the block. */
	uint32_t previous; /**< Offset of the record before it, when `hasPrevious`. */
	bool hasPrevious; /**< False for the first record of the block. */
	uint32_t inode; /**< Inode the record refers to. */
	uint32_t recordBytes; /**< Length of the record. */
};

/** @brief The block server the volume lives on. */
extern cock::user::block::Client disk;

/** @brief The block device that holds the volume. */
extern uint32_t device;

/** @brief True when the device is the whole disk, so nothing is written. */
extern bool readOnly;

/** @brief Client of the clock server, the source of file times. */
extern cock::user::Clock clock;

/** @brief True when the clock server answered at mount time. */
extern bool clockReady;

/** @brief Geometry and features of the volume, from its superblock. */
extern cock::abi::ext2::Superblock sb;

/** @brief The raw superblock, kept to write the counters and the state back. */
extern uint8_t superblockBuffer[cock::abi::ext2::SUPERBLOCK_BYTES];

/** @brief Buffer for one sector of the group descriptor table. */
extern uint8_t groupBuffer[SECTOR_BYTES];

/** @brief Sector held in `groupBuffer`, or `NOTHING_LOADED`. */
extern uint32_t groupLoaded;

/** @brief Buffer for one sector of the inode table. */
extern uint8_t inodeBuffer[SECTOR_BYTES];

/** @brief Sector held in `inodeBuffer`, or `NOTHING_LOADED`. */
extern uint32_t inodeLoaded;

/** @brief Buffer for one sector of a block of pointers. */
extern uint8_t pointerBuffer[SECTOR_BYTES];

/** @brief Sector held in `pointerBuffer`, or `NOTHING_LOADED`. */
extern uint32_t pointerLoaded;

/** @brief Buffer for one sector of a bitmap. */
extern uint8_t bitmapBuffer[SECTOR_BYTES];

/** @brief Sector held in `bitmapBuffer`, or `NOTHING_LOADED`. */
extern uint32_t bitmapLoaded;

/** @brief Buffer for one whole block of a directory. */
extern uint8_t directoryBuffer[cock::abi::ext2::MAX_BLOCK_BYTES];

/** @brief Block held in `directoryBuffer`, or `NOTHING_LOADED`. */
extern uint32_t directoryLoaded;

/** @brief Buffer for the partial sectors of file data. */
extern uint8_t dataBuffer[SECTOR_BYTES];

/** @brief Zeroes written over a new block. */
extern uint8_t zeros[cock::abi::ext2::MAX_BLOCK_BYTES];

/**
 * @brief Tells whether the volume accepts writes.
 *
 * @return True when the features allow it and the device is not the whole disk.
 */
inline bool writable() { return sb.writable && !readOnly; }

/**
 * @brief Tells whether a block number lies inside the data area of the volume.
 *
 * @param block The block number.
 * @return True for a block between the first data block and the end of the volume.
 */
inline bool validBlock(uint32_t block) {
	return block >= sb.firstDataBlock && block < sb.blocksCount;
}

/**
 * @brief Loads a sector into a buffer unless it is already there.
 *
 * @param sector Sector number on the device.
 * @param buffer The buffer to fill.
 * @param loaded The sector the buffer holds; updated, and reset on failure.
 * @return True when the buffer holds the sector, false on an I/O error.
 */
bool loadSector(uint32_t sector, uint8_t *buffer, uint32_t &loaded);

/**
 * @brief Writes a buffer to a sector.
 *
 * @param sector Sector number on the device.
 * @param buffer The data.
 * @param loaded The sector the buffer holds; reset on failure.
 * @return True on success, false on an I/O error.
 */
bool storeSector(uint32_t sector, const uint8_t *buffer, uint32_t &loaded);

} // namespace cock::ext

#endif // !EXT_VOLUME_HPP
