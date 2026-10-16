/**
 * @file ext2.hpp
 * @brief On-disk layout and pure helpers of the ext2 file system (also reads ext3 and ext4 volumes without extents).
 *
 * Everything here is parsing and encoding over byte buffers: no I/O and no dependencies, so the `ext2` server and
 * the host tests share it. All on-disk integers are little endian. Offsets named `SB_*`, `GD_*`, `INODE_*` and
 * `DIRENT_*` are byte offsets inside the superblock, a group descriptor, an inode and a directory entry. Supported:
 * revision 0 and 1, block sizes 1, 2 and 4 KiB, the `filetype` feature, sparse superblocks and large files (the size
 * is capped at 2 GiB minus one). Volumes with unsupported incompatible features are refused; a volume with a journal,
 * an error state or unknown read-only features is mounted read-only (`Superblock::writable` is false).
 */
#ifndef SHARED_COCK_ABI_EXT2_HPP
#define SHARED_COCK_ABI_EXT2_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::ext2 {

/** Size of a device sector in bytes. */
constexpr size_t SECTOR_BYTES = 512;
/** Byte offset of the superblock from the start of the partition. */
constexpr size_t SUPERBLOCK_OFFSET = 1024;
/** Size of the superblock in bytes. */
constexpr size_t SUPERBLOCK_BYTES = 1024;
/** First sector of the superblock (2). */
constexpr size_t SUPERBLOCK_SECTOR = SUPERBLOCK_OFFSET / SECTOR_BYTES;
/** Sectors the superblock occupies (2). */
constexpr size_t SUPERBLOCK_SECTORS = SUPERBLOCK_BYTES / SECTOR_BYTES;

/** Superblock field: total inodes (32 bit). */
constexpr size_t SB_INODES_COUNT_OFFSET = 0;
/** Superblock field: total blocks (32 bit). */
constexpr size_t SB_BLOCKS_COUNT_OFFSET = 4;
/** Superblock field: free blocks (32 bit). */
constexpr size_t SB_FREE_BLOCKS_OFFSET = 12;
/** Superblock field: free inodes (32 bit). */
constexpr size_t SB_FREE_INODES_OFFSET = 16;
/** Superblock field: first data block, 1 for 1 KiB blocks and 0 otherwise (32 bit). */
constexpr size_t SB_FIRST_DATA_BLOCK_OFFSET = 20;
/** Superblock field: block size is 1024 shifted left by this (32 bit). */
constexpr size_t SB_LOG_BLOCK_SIZE_OFFSET = 24;
/** Superblock field: blocks in each group (32 bit). */
constexpr size_t SB_BLOCKS_PER_GROUP_OFFSET = 32;
/** Superblock field: inodes in each group (32 bit). */
constexpr size_t SB_INODES_PER_GROUP_OFFSET = 40;
/** Superblock field: last write time, Unix seconds (32 bit). */
constexpr size_t SB_WRITE_TIME_OFFSET = 48;
/** Superblock field: magic number (16 bit). */
constexpr size_t SB_MAGIC_OFFSET = 56;
/** Superblock field: file system state flags (16 bit). */
constexpr size_t SB_STATE_OFFSET = 58;
/** Superblock field: revision level (32 bit). */
constexpr size_t SB_REVISION_OFFSET = 76;
/** Superblock field: first non reserved inode, revision 1 (32 bit). */
constexpr size_t SB_FIRST_INODE_OFFSET = 84;
/** Superblock field: inode size in bytes, revision 1 (16 bit). */
constexpr size_t SB_INODE_SIZE_OFFSET = 88;
/** Superblock field: compatible feature flags (32 bit). */
constexpr size_t SB_COMPAT_OFFSET = 92;
/** Superblock field: incompatible feature flags (32 bit). */
constexpr size_t SB_INCOMPAT_OFFSET = 96;
/** Superblock field: read-only compatible feature flags (32 bit). */
constexpr size_t SB_RO_COMPAT_OFFSET = 100;
/** Value of the superblock magic number. */
constexpr uint16_t MAGIC = 0xEF53;
/** First revision with variable inode size and feature flags. */
constexpr uint32_t REVISION_DYNAMIC = 1;
/** Smallest block size; the unit of the log block size field. */
constexpr uint32_t BASE_BLOCK_BYTES = 1024;
/** Largest accepted log block size (4 KiB blocks). */
constexpr uint32_t MAX_LOG_BLOCK_SIZE = 2;
/** Largest accepted block size in bytes (4096). */
constexpr uint32_t MAX_BLOCK_BYTES = BASE_BLOCK_BYTES << MAX_LOG_BLOCK_SIZE;
/** Block pointers that fit in the largest block (1024). */
constexpr uint32_t MAX_POINTERS_PER_BLOCK = MAX_BLOCK_BYTES / sizeof(uint32_t);
/** Inode size of revision 0 and the smallest valid size. */
constexpr uint32_t LEGACY_INODE_BYTES = 128;
/** Largest accepted inode size. */
constexpr uint32_t MAX_INODE_BYTES = 512;
/** First non reserved inode of revision 0. */
constexpr uint32_t LEGACY_FIRST_INODE = 11;
/** Bits in a byte; used by the bitmap helpers. */
constexpr uint32_t BITS_PER_BYTE = 8;

/** Incompatible feature: compression. Not supported. */
constexpr uint32_t INCOMPAT_COMPRESSION = 0x00001;
/** Incompatible feature: directory entries carry a file type. The only one supported. */
constexpr uint32_t INCOMPAT_FILETYPE = 0x00002;
/** Incompatible feature: the journal needs recovery. Not supported. */
constexpr uint32_t INCOMPAT_RECOVER = 0x00004;
/** Incompatible feature: external journal device. Not supported. */
constexpr uint32_t INCOMPAT_JOURNAL_DEVICE = 0x00008;
/** Incompatible feature: meta block groups. Not supported. */
constexpr uint32_t INCOMPAT_META_BLOCK_GROUPS = 0x00010;
/** Incompatible feature: extent trees (ext4). Not supported. */
constexpr uint32_t INCOMPAT_EXTENTS = 0x00040;
/** Incompatible feature: 64 bit block numbers. Not supported. */
constexpr uint32_t INCOMPAT_64BIT = 0x00080;
/** Incompatible feature: multiple mount protection. Not supported. */
constexpr uint32_t INCOMPAT_MULTIPLE_MOUNT_PROTECTION = 0x00100;
/** Incompatible feature: flexible block groups. Not supported. */
constexpr uint32_t INCOMPAT_FLEX_BLOCK_GROUPS = 0x00200;
/** Incompatible feature: extended attributes in inodes. Not supported. */
constexpr uint32_t INCOMPAT_EXTENDED_ATTRIBUTE_INODES = 0x00400;
/** Incompatible feature: data in directory entries. Not supported. */
constexpr uint32_t INCOMPAT_DIRECTORY_DATA = 0x01000;
/** Incompatible feature: checksum seed in the superblock. Not supported. */
constexpr uint32_t INCOMPAT_CHECKSUM_SEED = 0x02000;
/** Incompatible feature: directories over 2 GiB or three level hashes. Not supported. */
constexpr uint32_t INCOMPAT_LARGE_DIRECTORIES = 0x04000;
/** Incompatible feature: data inside the inode. Not supported. */
constexpr uint32_t INCOMPAT_INLINE_DATA = 0x08000;
/** Incompatible feature: encrypted files. Not supported. */
constexpr uint32_t INCOMPAT_ENCRYPTION = 0x10000;
/** Incompatible features this implementation understands; any other bit makes the mount fail. */
constexpr uint32_t SUPPORTED_INCOMPAT = INCOMPAT_FILETYPE;
/** Compatible feature: the volume has a journal. The journal is not replayed, so the volume is mounted read-only. */
constexpr uint32_t COMPAT_HAS_JOURNAL = 0x00004;
/** Read-only compatible feature: sparse superblock copies. */
constexpr uint32_t RO_COMPAT_SPARSE_SUPER = 0x00001;
/** Read-only compatible feature: files over 2 GiB exist. */
constexpr uint32_t RO_COMPAT_LARGE_FILE = 0x00002;
/** Read-only compatible features understood for writing; any other bit forces a read-only mount. */
constexpr uint32_t SUPPORTED_RO_COMPAT = RO_COMPAT_SPARSE_SUPER | RO_COMPAT_LARGE_FILE;
/** State flag: the volume was cleanly unmounted. */
constexpr uint32_t STATE_VALID = 0x0001;
/** State flag: errors were detected; the volume is mounted read-only. */
constexpr uint32_t STATE_ERROR = 0x0002;

/** Size of one block group descriptor. */
constexpr size_t GROUP_DESCRIPTOR_BYTES = 32;
/** Group descriptor field: block of the block bitmap (32 bit). */
constexpr size_t GD_BLOCK_BITMAP_OFFSET = 0;
/** Group descriptor field: block of the inode bitmap (32 bit). */
constexpr size_t GD_INODE_BITMAP_OFFSET = 4;
/** Group descriptor field: first block of the inode table (32 bit). */
constexpr size_t GD_INODE_TABLE_OFFSET = 8;
/** Group descriptor field: free blocks in the group (16 bit). */
constexpr size_t GD_FREE_BLOCKS_OFFSET = 12;
/** Group descriptor field: free inodes in the group (16 bit). */
constexpr size_t GD_FREE_INODES_OFFSET = 14;

/** Inode number of the root directory. Inode numbers start at 1. */
constexpr uint32_t ROOT_INODE = 2;
/** Inode field: type and permission bits (16 bit). */
constexpr size_t INODE_MODE_OFFSET = 0;
/** Inode field: size in bytes, low 32 bits. */
constexpr size_t INODE_SIZE_OFFSET = 4;
/** Inode field: access time, Unix seconds (32 bit). */
constexpr size_t INODE_ACCESS_TIME_OFFSET = 8;
/** Inode field: change time, Unix seconds (32 bit). */
constexpr size_t INODE_CHANGE_TIME_OFFSET = 12;
/** Inode field: modification time, Unix seconds (32 bit). */
constexpr size_t INODE_MODIFY_TIME_OFFSET = 16;
/** Inode field: deletion time, Unix seconds (32 bit); non zero marks a freed inode. */
constexpr size_t INODE_DELETION_TIME_OFFSET = 20;
/** Value stored in the deletion time of a freed inode (a fixed non zero time, 2001-09-09). */
constexpr uint32_t DELETION_TIME_MARK = 1000000000;
/** Inode field: hard link count (16 bit). */
constexpr size_t INODE_LINKS_OFFSET = 26;
/** Inode field: number of 512 byte sectors in use, including metadata blocks (32 bit). */
constexpr size_t INODE_SECTORS_OFFSET = 28;
/** Inode field: flags (32 bit). */
constexpr size_t INODE_FLAGS_OFFSET = 32;
/** Inode field: the 15 block pointers, or the target of a fast symbolic link. */
constexpr size_t INODE_BLOCKS_OFFSET = 40;
/** Inode field: high 32 bits of the size of a regular file. */
constexpr size_t INODE_SIZE_HIGH_OFFSET = 108;
/** Block pointers in an inode: 12 direct and 3 indirect. */
constexpr uint32_t INODE_BLOCK_SLOTS = 15;
/** Direct block pointers. */
constexpr uint32_t DIRECT_BLOCKS = 12;
/** Index of the pointer to the single indirect block. */
constexpr uint32_t SINGLE_INDIRECT_SLOT = 12;
/** Index of the pointer to the double indirect block. */
constexpr uint32_t DOUBLE_INDIRECT_SLOT = 13;
/** Index of the pointer to the triple indirect block. */
constexpr uint32_t TRIPLE_INDIRECT_SLOT = 14;
/** Inode flag: the block area holds an extent tree (not supported). */
constexpr uint32_t INODE_EXTENTS_FLAG = 0x00080000;
/** Inode flag: the data lives in the inode (not supported). */
constexpr uint32_t INODE_INLINE_DATA_FLAG = 0x10000000;
/** Mask of the file type bits of the mode. */
constexpr uint32_t MODE_TYPE_MASK = 0xF000;
/** Mode type: directory. */
constexpr uint32_t MODE_DIRECTORY = 0x4000;
/** Mode type: regular file. */
constexpr uint32_t MODE_REGULAR = 0x8000;
/** Mode type: symbolic link. */
constexpr uint32_t MODE_SYMLINK = 0xA000;
/** Permission bits rw-r--r-- (octal 0644) given to new files. */
constexpr uint32_t MODE_PERMISSIONS_644 = 0x01A4;
/** Mode of a newly created regular file. */
constexpr uint32_t MODE_NEW_FILE = MODE_REGULAR | MODE_PERMISSIONS_644;
/** Link count of a new file. */
constexpr uint32_t NEW_FILE_LINKS = 1;
/** Most hard links to one inode; one more is refused. */
constexpr uint32_t MAX_LINKS = 65000;
/** Permission bits rwxr-xr-x (octal 0755) given to new directories. */
constexpr uint32_t MODE_PERMISSIONS_755 = 0x01ED;
/** Mode of a newly created directory. */
constexpr uint32_t MODE_NEW_DIRECTORY = MODE_DIRECTORY | MODE_PERMISSIONS_755;
/** Link count of a new directory: its name and its own `.` entry. */
constexpr uint32_t NEW_DIRECTORY_LINKS = 2;
/** Group descriptor field: directories in the group (16 bit). */
constexpr size_t GD_USED_DIRECTORIES_OFFSET = 16;
/** Directory entry file type: directory. */
constexpr uint8_t DIRENT_TYPE_DIRECTORY = 2;
/** Directory entry file type: symbolic link. */
constexpr uint8_t DIRENT_TYPE_SYMLINK = 7;
/** Name of the entry that points to the directory itself. */
constexpr char CURRENT_ENTRY_NAME[] = ".";
/** Name of the entry that points to the parent directory. */
constexpr char PARENT_ENTRY_NAME[] = "..";
/** Length of `CURRENT_ENTRY_NAME`. */
constexpr uint32_t CURRENT_ENTRY_BYTES = 1;
/** Length of `PARENT_ENTRY_NAME`. */
constexpr uint32_t PARENT_ENTRY_BYTES = 2;
/** Largest file size reported; bigger files are capped to it. */
constexpr uint32_t MAX_FILE_BYTES = 0x7FFFFFFF;
/** Longest symbolic link target stored inside the inode (60 bytes). */
constexpr uint32_t FAST_SYMLINK_BYTES = INODE_BLOCK_SLOTS * sizeof(uint32_t);

/** Fixed part of a directory entry before the name. */
constexpr size_t DIRENT_HEADER_BYTES = 8;
/** Directory entry field: record length (16 bit). */
constexpr size_t DIRENT_RECORD_OFFSET = 4;
/** Directory entry field: name length (8 bit with `filetype`, otherwise 16 bit). */
constexpr size_t DIRENT_NAME_LENGTH_OFFSET = 6;
/** Directory entry field: file type (8 bit, only with `filetype`). */
constexpr size_t DIRENT_TYPE_OFFSET = 7;
/** Directory entry field: start of the name (not terminated). */
constexpr size_t DIRENT_NAME_OFFSET = 8;
/** Records are multiples of this many bytes. */
constexpr uint32_t DIRENT_ALIGNMENT = 4;
/** Directory entry file type: regular file. */
constexpr uint8_t DIRENT_TYPE_REGULAR = 1;
/** Value returned by `findClearBit` when no bit is clear. */
constexpr uint32_t NO_BIT = 0xFFFFFFFF;
/** A bitmap byte with every bit set; lets `findClearBit` skip eight bits. */
constexpr uint8_t FULL_BYTE = 0xFF;
/** A single set bit used to build masks. */
constexpr uint32_t FIRST_BIT = 1;
/** Bits that one sector of a bitmap covers (4096). */
constexpr uint32_t BITS_PER_BITMAP_SECTOR = uint32_t(SECTOR_BYTES) * BITS_PER_BYTE;

/** Shift that places a byte at bits 8 to 15. */
constexpr unsigned SHIFT_8 = 8;
/** Shift that places a byte at bits 16 to 23. */
constexpr unsigned SHIFT_16 = 16;
/** Shift that places a byte at bits 24 to 31. */
constexpr unsigned SHIFT_24 = 24;
/** Index of the third byte of a 32 bit field. */
constexpr size_t THIRD_BYTE = 2;
/** Index of the fourth byte of a 32 bit field. */
constexpr size_t FOURTH_BYTE = 3;

/** Reason why `parseSuperblock` refused a volume. */
enum class Error : uint32_t {
	/** The superblock is valid and the geometry is consistent. */
	NONE = 0,
	/** The magic number is not 0xEF53. */
	BAD_MAGIC,
	/** The block size is not 1, 2 or 4 KiB. */
	BAD_BLOCK_SIZE,
	/** Counts, group sizes or the first data block are inconsistent, or the volume is larger than the partition. */
	BAD_GEOMETRY,
	/** The inode size is not a power of two between 128 and 512, or exceeds the block size. */
	BAD_INODE_SIZE,
	/** The volume uses an incompatible feature that is not supported; see `Superblock::unsupported`. */
	UNSUPPORTED_FEATURES,
};

/** File type decoded from the inode mode. */
enum class InodeType : uint32_t {
	/** Anything else (device, socket, fifo). */
	OTHER = 0,
	/** Regular file. */
	REGULAR,
	/** Directory. */
	DIRECTORY,
	/** Symbolic link. */
	SYMLINK,
};

/** Decoded superblock with the values the server needs. */
struct Superblock {
	/** Total inodes. */
	uint32_t inodesCount;
	/** Total blocks. */
	uint32_t blocksCount;
	/** Block number where block group 0 starts. */
	uint32_t firstDataBlock;
	/** Block size in bytes: 1024, 2048 or 4096. */
	uint32_t blockBytes;
	/** Blocks per group, 1 to `blockBytes * 8`. */
	uint32_t blocksPerGroup;
	/** Inodes per group, 1 to `blockBytes * 8`. */
	uint32_t inodesPerGroup;
	/** Size of an inode in bytes. */
	uint32_t inodeBytes;
	/** Number of block groups, derived from the block count. */
	uint32_t groupCount;
	/** Compatible feature flags (0 on revision 0). */
	uint32_t compat;
	/** Incompatible feature flags (0 on revision 0). */
	uint32_t incompat;
	/** Read-only compatible feature flags (0 on revision 0). */
	uint32_t roCompat;
	/** Incompatible flags that are set and not supported. */
	uint32_t unsupported;
	/** Free blocks counted by the superblock. */
	uint32_t freeBlocks;
	/** Free inodes counted by the superblock. */
	uint32_t freeInodes;
	/** State flags (`STATE_VALID`, `STATE_ERROR`). */
	uint32_t state;
	/** True when directory entries carry a file type. */
	bool fileType;
	/** True when the volume may be written: no journal, no error state, cleanly unmounted and only known read-only features. */
	bool writable;

	/**
	 * @brief Sectors in one block.
	 * @return `blockBytes / 512`.
	 */
	uint32_t sectorsPerBlock() const { return blockBytes / uint32_t(SECTOR_BYTES); }
	/**
	 * @brief Block pointers an indirect block holds.
	 * @return `blockBytes / 4`.
	 */
	uint32_t pointersPerBlock() const { return blockBytes / uint32_t(sizeof(uint32_t)); }
	/**
	 * @brief Sector of the group descriptor table, which follows the superblock.
	 * @return First sector of the table, relative to the partition.
	 */
	uint32_t groupTableSector() const { return (firstDataBlock + 1) * sectorsPerBlock(); }
};

/** Decoded inode. Only the fields the server uses are kept. */
struct Inode {
	/** File type from the mode. */
	InodeType type;
	/** Size in bytes, capped at `MAX_FILE_BYTES` for regular files. */
	uint32_t size;
	/** Hard link count. */
	uint32_t links;
	/** 512 byte sectors in use. */
	uint32_t sectors;
	/** Inode flags. */
	uint32_t flags;
	/** Block pointers: 12 direct, then single, double and triple indirect; for a fast symlink the raw target text. */
	uint32_t blocks[INODE_BLOCK_SLOTS];
};

/** Decoded directory entry; the name points into the block that was parsed. */
struct DirItem {
	/** Inode number; 0 marks a deleted or unused entry. */
	uint32_t inode;
	/** Length of the whole record, a multiple of 4 and at least 8. */
	uint32_t recordBytes;
	/** Length of the name. */
	uint32_t nameBytes;
	/** Name bytes, not null terminated, valid while the block buffer lives. */
	const uint8_t *name;
};

/** Path through the block pointer tree to the data block of one file block. */
struct BlockPath {
	/** Number of indirect levels: 0 direct, 1 single, 2 double or 3 triple. */
	uint32_t depth;
	/** Index to take at each level, starting with the inode slot; only `depth + 1` entries are valid. */
	uint32_t slots[4];
};

/** Pairs a feature bit with a printable name for diagnostics. */
struct FeatureName {
	/** Feature flag. */
	uint32_t bit;
	/** Short name as used by the ext2 tools. */
	const char *name;
};

/** Names of the incompatible features that can be reported when a mount is refused. */
constexpr FeatureName INCOMPAT_NAMES[] = {
	{INCOMPAT_COMPRESSION, "compression"},
	{INCOMPAT_RECOVER, "needs_recovery"},
	{INCOMPAT_JOURNAL_DEVICE, "journal_dev"},
	{INCOMPAT_META_BLOCK_GROUPS, "meta_bg"},
	{INCOMPAT_EXTENTS, "extents"},
	{INCOMPAT_64BIT, "64bit"},
	{INCOMPAT_MULTIPLE_MOUNT_PROTECTION, "mmp"},
	{INCOMPAT_FLEX_BLOCK_GROUPS, "flex_bg"},
	{INCOMPAT_EXTENDED_ATTRIBUTE_INODES, "ea_inode"},
	{INCOMPAT_DIRECTORY_DATA, "dirdata"},
	{INCOMPAT_CHECKSUM_SEED, "csum_seed"},
	{INCOMPAT_LARGE_DIRECTORIES, "large_dir"},
	{INCOMPAT_INLINE_DATA, "inline_data"},
	{INCOMPAT_ENCRYPTION, "encrypt"},
};
/** Number of entries of `INCOMPAT_NAMES`. */
constexpr size_t INCOMPAT_NAME_COUNT = sizeof(INCOMPAT_NAMES) / sizeof(INCOMPAT_NAMES[0]);
/** Name returned for a bit that is not in `INCOMPAT_NAMES`. */
constexpr char UNKNOWN_FEATURE_NAME[] = "unknown";

namespace detail {

/**
 * @brief Reads a little endian 16 bit integer.
 * @param bytes At least 2 readable bytes.
 * @return The value.
 */
inline uint16_t readLe16(const uint8_t *bytes) {
	return uint16_t(uint16_t(bytes[0]) | uint16_t(uint16_t(bytes[1]) << SHIFT_8));
}

/**
 * @brief Reads a little endian 32 bit integer.
 * @param bytes At least 4 readable bytes.
 * @return The value.
 */
inline uint32_t readLe32(const uint8_t *bytes) {
	return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << SHIFT_8) |
		   (uint32_t(bytes[THIRD_BYTE]) << SHIFT_16) |
		   (uint32_t(bytes[FOURTH_BYTE]) << SHIFT_24);
}

/**
 * @brief Tells whether a value is a power of two.
 * @param value Number to test.
 * @return True for 1, 2, 4, 8 and so on; false for 0.
 */
inline bool isPowerOfTwo(uint32_t value) { return value != 0 && (value & (value - 1)) == 0; }

/**
 * @brief Writes the low 16 bits of a value in little endian.
 * @param bytes At least 2 writable bytes.
 * @param value Value to store; higher bits are dropped.
 */
inline void writeLe16(uint8_t *bytes, uint32_t value) {
	bytes[0] = uint8_t(value);
	bytes[1] = uint8_t(value >> SHIFT_8);
}

/**
 * @brief Writes a 32 bit value in little endian.
 * @param bytes At least 4 writable bytes.
 * @param value Value to store.
 */
inline void writeLe32(uint8_t *bytes, uint32_t value) {
	bytes[0] = uint8_t(value);
	bytes[1] = uint8_t(value >> SHIFT_8);
	bytes[THIRD_BYTE] = uint8_t(value >> SHIFT_16);
	bytes[FOURTH_BYTE] = uint8_t(value >> SHIFT_24);
}

} // namespace detail

/**
 * @brief Printable name of an incompatible feature bit.
 * @param bit One feature flag.
 * @return Its name, or "unknown" when the bit is not listed.
 */
inline const char *featureName(uint32_t bit) {
	for (size_t index = 0; index < INCOMPAT_NAME_COUNT; index++)
		if (INCOMPAT_NAMES[index].bit == bit) return INCOMPAT_NAMES[index].name;
	return UNKNOWN_FEATURE_NAME;
}

/**
 * @brief Decodes and validates the superblock.
 *
 * Checks the magic, block size, inode size, group geometry, that the volume fits in the partition and that the
 * inode count is reachable from the groups. Unsupported incompatible features are reported after the fields are
 * filled in, so the caller can print `Superblock::unsupported`.
 * @param bytes The 1024 bytes of the superblock.
 * @param partitionSectors Size of the partition in sectors.
 * @param sb Receives the decoded values; unspecified when an error other than `UNSUPPORTED_FEATURES` is returned.
 * @return `Error::NONE` or the first problem found.
 */
inline Error parseSuperblock(const uint8_t *bytes, uint64_t partitionSectors,
							 Superblock &sb) {
	if (detail::readLe16(bytes + SB_MAGIC_OFFSET) != MAGIC) return Error::BAD_MAGIC;

	uint32_t logBlockSize = detail::readLe32(bytes + SB_LOG_BLOCK_SIZE_OFFSET);
	if (logBlockSize > MAX_LOG_BLOCK_SIZE) return Error::BAD_BLOCK_SIZE;

	sb.blockBytes = BASE_BLOCK_BYTES << logBlockSize;
	sb.inodesCount = detail::readLe32(bytes + SB_INODES_COUNT_OFFSET);
	sb.blocksCount = detail::readLe32(bytes + SB_BLOCKS_COUNT_OFFSET);
	sb.firstDataBlock = detail::readLe32(bytes + SB_FIRST_DATA_BLOCK_OFFSET);
	sb.blocksPerGroup = detail::readLe32(bytes + SB_BLOCKS_PER_GROUP_OFFSET);
	sb.inodesPerGroup = detail::readLe32(bytes + SB_INODES_PER_GROUP_OFFSET);

	uint32_t revision = detail::readLe32(bytes + SB_REVISION_OFFSET);
	sb.inodeBytes = LEGACY_INODE_BYTES;
	sb.compat = 0;
	sb.incompat = 0;
	sb.roCompat = 0;
	if (revision >= REVISION_DYNAMIC) {
		sb.inodeBytes = detail::readLe16(bytes + SB_INODE_SIZE_OFFSET);
		sb.compat = detail::readLe32(bytes + SB_COMPAT_OFFSET);
		sb.incompat = detail::readLe32(bytes + SB_INCOMPAT_OFFSET);
		sb.roCompat = detail::readLe32(bytes + SB_RO_COMPAT_OFFSET);
	}

	sb.unsupported = sb.incompat & ~SUPPORTED_INCOMPAT;
	sb.fileType = (sb.incompat & INCOMPAT_FILETYPE) != 0;
	sb.freeBlocks = detail::readLe32(bytes + SB_FREE_BLOCKS_OFFSET);
	sb.freeInodes = detail::readLe32(bytes + SB_FREE_INODES_OFFSET);
	sb.state = detail::readLe16(bytes + SB_STATE_OFFSET);
	sb.writable = (sb.roCompat & ~SUPPORTED_RO_COMPAT) == 0 &&
				  (sb.compat & COMPAT_HAS_JOURNAL) == 0 && (sb.state & STATE_VALID) != 0 &&
				  (sb.state & STATE_ERROR) == 0;
	if (sb.unsupported != 0) return Error::UNSUPPORTED_FEATURES;

	if (!detail::isPowerOfTwo(sb.inodeBytes) || sb.inodeBytes < LEGACY_INODE_BYTES ||
		sb.inodeBytes > MAX_INODE_BYTES || sb.inodeBytes > sb.blockBytes)
		return Error::BAD_INODE_SIZE;

	uint32_t expectedFirst = sb.blockBytes == BASE_BLOCK_BYTES ? 1 : 0;
	uint32_t maxPerGroup = sb.blockBytes * BITS_PER_BYTE;
	if (sb.firstDataBlock != expectedFirst || sb.blocksPerGroup == 0 ||
		sb.blocksPerGroup > maxPerGroup || sb.inodesPerGroup == 0 ||
		sb.inodesPerGroup > maxPerGroup || sb.blocksCount <= sb.firstDataBlock ||
		sb.inodesCount < ROOT_INODE)
		return Error::BAD_GEOMETRY;
	if (uint64_t(sb.blocksCount) * sb.sectorsPerBlock() > partitionSectors)
		return Error::BAD_GEOMETRY;

	uint32_t usable = sb.blocksCount - sb.firstDataBlock;
	sb.groupCount = usable / sb.blocksPerGroup + (usable % sb.blocksPerGroup != 0 ? 1 : 0);
	if (uint64_t(sb.groupCount) * sb.inodesPerGroup < sb.inodesCount)
		return Error::BAD_GEOMETRY;
	return Error::NONE;
}

/**
 * @brief Decodes an inode.
 * @param raw At least 128 bytes of the inode.
 * @return The inode. Nothing is validated here; sizes of regular files above 2 GiB minus one are capped.
 */
inline Inode parseInode(const uint8_t *raw) {
	Inode inode = {};
	uint32_t mode = detail::readLe16(raw + INODE_MODE_OFFSET) & MODE_TYPE_MASK;
	inode.type = mode == MODE_DIRECTORY ? InodeType::DIRECTORY
				 : mode == MODE_REGULAR ? InodeType::REGULAR
				 : mode == MODE_SYMLINK ? InodeType::SYMLINK
										: InodeType::OTHER;
	inode.size = detail::readLe32(raw + INODE_SIZE_OFFSET);
	if (inode.type == InodeType::REGULAR &&
		(detail::readLe32(raw + INODE_SIZE_HIGH_OFFSET) != 0 || inode.size > MAX_FILE_BYTES))
		inode.size = MAX_FILE_BYTES;
	inode.links = detail::readLe16(raw + INODE_LINKS_OFFSET);
	inode.sectors = detail::readLe32(raw + INODE_SECTORS_OFFSET);
	inode.flags = detail::readLe32(raw + INODE_FLAGS_OFFSET);
	for (uint32_t slot = 0; slot < INODE_BLOCK_SLOTS; slot++)
		inode.blocks[slot] =
			detail::readLe32(raw + INODE_BLOCKS_OFFSET + size_t(slot) * sizeof(uint32_t));
	return inode;
}

/**
 * @brief Decodes the directory entry at an offset of a directory block, with bounds checks.
 * @param block Directory block.
 * @param blockBytes Size of the block.
 * @param offset Byte offset of the entry; must leave room for the 8 byte header.
 * @param fileType True when the volume has the `filetype` feature (the name length is then 8 bit).
 * @param item Receives the entry; its name points into `block`.
 * @return False when the record is shorter than 8 bytes, not a multiple of 4, passes the end of the block or is too short for its name.
 */
inline bool parseDirItem(const uint8_t *block, uint32_t blockBytes, uint32_t offset,
						 bool fileType, DirItem &item) {
	if (offset > blockBytes || blockBytes - offset < DIRENT_HEADER_BYTES) return false;

	const uint8_t *raw = block + offset;
	uint32_t record = detail::readLe16(raw + DIRENT_RECORD_OFFSET);
	if (record < DIRENT_HEADER_BYTES || record % DIRENT_ALIGNMENT != 0 ||
		record > blockBytes - offset)
		return false;

	uint32_t nameBytes = fileType ? raw[DIRENT_NAME_LENGTH_OFFSET]
								  : detail::readLe16(raw + DIRENT_NAME_LENGTH_OFFSET);
	if (nameBytes > record - DIRENT_HEADER_BYTES) return false;

	item.inode = detail::readLe32(raw);
	item.recordBytes = record;
	item.nameBytes = nameBytes;
	item.name = raw + DIRENT_NAME_OFFSET;
	return true;
}

/**
 * @brief Record size needed for a name.
 * @param nameBytes Length of the name.
 * @return Header plus name, rounded up to a multiple of 4.
 */
inline uint32_t direntBytes(uint32_t nameBytes) {
	return (uint32_t(DIRENT_HEADER_BYTES) + nameBytes + DIRENT_ALIGNMENT - 1) /
		   DIRENT_ALIGNMENT * DIRENT_ALIGNMENT;
}

/**
 * @brief Writes a directory entry.
 * @param raw Destination with room for `recordBytes` bytes.
 * @param inode Inode number.
 * @param recordBytes Length of the record (at least `direntBytes(nameBytes)`).
 * @param name Name bytes.
 * @param nameBytes Length of the name.
 * @param fileType True to store the file type and an 8 bit name length.
 * @param type File type (`DIRENT_TYPE_*`), used only when `fileType` is true.
 */
inline void encodeDirItem(uint8_t *raw, uint32_t inode, uint32_t recordBytes,
						  const char *name, uint32_t nameBytes, bool fileType,
						  uint8_t type) {
	detail::writeLe32(raw, inode);
	detail::writeLe16(raw + DIRENT_RECORD_OFFSET, recordBytes);
	if (fileType) {
		raw[DIRENT_NAME_LENGTH_OFFSET] = uint8_t(nameBytes);
		raw[DIRENT_TYPE_OFFSET] = type;
	} else {
		detail::writeLe16(raw + DIRENT_NAME_LENGTH_OFFSET, nameBytes);
	}
	for (uint32_t index = 0; index < nameBytes; index++)
		raw[DIRENT_NAME_OFFSET + index] = uint8_t(name[index]);
}

/**
 * @brief Fills the first block of a new directory with the `.` and `..` entries.
 * @param block Block buffer; cleared first.
 * @param blockBytes Size of the block.
 * @param self Inode number of the new directory.
 * @param parent Inode number of its parent.
 * @param fileType True when the volume has the `filetype` feature.
 */
inline void initDirectoryBlock(uint8_t *block, uint32_t blockBytes, uint32_t self,
							   uint32_t parent, bool fileType) {
	for (uint32_t index = 0; index < blockBytes; index++)
		block[index] = 0;
	uint32_t dotBytes = direntBytes(CURRENT_ENTRY_BYTES);
	encodeDirItem(block, self, dotBytes, CURRENT_ENTRY_NAME, CURRENT_ENTRY_BYTES, fileType,
				  DIRENT_TYPE_DIRECTORY);
	encodeDirItem(block + dotBytes, parent, blockBytes - dotBytes, PARENT_ENTRY_NAME,
				  PARENT_ENTRY_BYTES, fileType, DIRENT_TYPE_DIRECTORY);
}

/**
 * @brief Mask of a bit inside its bitmap byte.
 * @param index Bit index in the bitmap.
 * @return A byte with only that bit set.
 */
inline uint8_t bitMask(uint32_t index) { return uint8_t(FIRST_BIT << (index % BITS_PER_BYTE)); }

/**
 * @brief Reads a bit of a bitmap.
 * @param bitmap Bitmap bytes.
 * @param index Bit index; must be inside the bitmap.
 * @return True when the bit is 1.
 */
inline bool bitIsSet(const uint8_t *bitmap, uint32_t index) {
	return (bitmap[index / BITS_PER_BYTE] & bitMask(index)) != 0;
}

/**
 * @brief Sets a bit of a bitmap.
 * @param bitmap Bitmap bytes.
 * @param index Bit index; must be inside the bitmap.
 */
inline void setBit(uint8_t *bitmap, uint32_t index) {
	bitmap[index / BITS_PER_BYTE] = uint8_t(bitmap[index / BITS_PER_BYTE] | bitMask(index));
}

/**
 * @brief Clears a bit of a bitmap.
 * @param bitmap Bitmap bytes.
 * @param index Bit index; must be inside the bitmap.
 */
inline void clearBit(uint8_t *bitmap, uint32_t index) {
	bitmap[index / BITS_PER_BYTE] = uint8_t(bitmap[index / BITS_PER_BYTE] & ~bitMask(index));
}

/**
 * @brief Finds the first free (zero) bit.
 * @param bitmap Bitmap bytes.
 * @param from First index to look at.
 * @param limit One past the last index to look at.
 * @return The index of the first zero bit in `[from, limit)`, or `NO_BIT`.
 */
inline uint32_t findClearBit(const uint8_t *bitmap, uint32_t from, uint32_t limit) {
	uint32_t index = from;
	while (index < limit) {
		if (index % BITS_PER_BYTE == 0 && bitmap[index / BITS_PER_BYTE] == FULL_BYTE) {
			index += BITS_PER_BYTE;
			continue;
		}
		if (!bitIsSet(bitmap, index)) return index;
		index++;
	}
	return NO_BIT;
}

/**
 * @brief Computes how to reach a file block through the pointer tree of an inode.
 * @param fileBlock Index of the block inside the file, counted from 0.
 * @param perBlock Pointers per indirect block (`Superblock::pointersPerBlock`), 1 to `MAX_POINTERS_PER_BLOCK`.
 * @param path Receives the depth and the slot to take at each level; cleared first.
 * @return False when `perBlock` is invalid or `fileBlock` is beyond what triple indirection can address.
 */
inline bool blockPath(uint32_t fileBlock, uint32_t perBlock, BlockPath &path) {
	path = {};
	if (perBlock == 0 || perBlock > MAX_POINTERS_PER_BLOCK) return false;
	if (fileBlock < DIRECT_BLOCKS) {
		path.slots[0] = fileBlock;
		return true;
	}

	uint32_t rest = fileBlock - DIRECT_BLOCKS;
	uint32_t single = perBlock;
	uint32_t twice = single * perBlock;
	uint32_t thrice = twice * perBlock;
	if (rest < single) {
		path.depth = 1;
		path.slots[0] = SINGLE_INDIRECT_SLOT;
		path.slots[1] = rest;
		return true;
	}
	rest -= single;
	if (rest < twice) {
		path.depth = 2;
		path.slots[0] = DOUBLE_INDIRECT_SLOT;
		path.slots[1] = rest / perBlock;
		path.slots[2] = rest % perBlock;
		return true;
	}
	rest -= twice;
	if (rest >= thrice) return false;

	path.depth = 3;
	path.slots[0] = TRIPLE_INDIRECT_SLOT;
	path.slots[1] = rest / twice;
	path.slots[2] = (rest / perBlock) % perBlock;
	path.slots[3] = rest % perBlock;
	return true;
}

} // namespace cock::abi::ext2

#endif // !SHARED_COCK_ABI_EXT2_HPP
