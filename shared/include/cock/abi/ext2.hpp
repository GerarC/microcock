#ifndef SHARED_COCK_ABI_EXT2_HPP
#define SHARED_COCK_ABI_EXT2_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::ext2 {

constexpr size_t SECTOR_BYTES = 512;
constexpr size_t SUPERBLOCK_OFFSET = 1024;
constexpr size_t SUPERBLOCK_BYTES = 1024;
constexpr size_t SUPERBLOCK_SECTOR = SUPERBLOCK_OFFSET / SECTOR_BYTES;
constexpr size_t SUPERBLOCK_SECTORS = SUPERBLOCK_BYTES / SECTOR_BYTES;

constexpr size_t SB_INODES_COUNT_OFFSET = 0;
constexpr size_t SB_BLOCKS_COUNT_OFFSET = 4;
constexpr size_t SB_FREE_BLOCKS_OFFSET = 12;
constexpr size_t SB_FREE_INODES_OFFSET = 16;
constexpr size_t SB_FIRST_DATA_BLOCK_OFFSET = 20;
constexpr size_t SB_LOG_BLOCK_SIZE_OFFSET = 24;
constexpr size_t SB_BLOCKS_PER_GROUP_OFFSET = 32;
constexpr size_t SB_INODES_PER_GROUP_OFFSET = 40;
constexpr size_t SB_MAGIC_OFFSET = 56;
constexpr size_t SB_STATE_OFFSET = 58;
constexpr size_t SB_REVISION_OFFSET = 76;
constexpr size_t SB_FIRST_INODE_OFFSET = 84;
constexpr size_t SB_INODE_SIZE_OFFSET = 88;
constexpr size_t SB_COMPAT_OFFSET = 92;
constexpr size_t SB_INCOMPAT_OFFSET = 96;
constexpr size_t SB_RO_COMPAT_OFFSET = 100;
constexpr uint16_t MAGIC = 0xEF53;
constexpr uint32_t REVISION_DYNAMIC = 1;
constexpr uint32_t BASE_BLOCK_BYTES = 1024;
constexpr uint32_t MAX_LOG_BLOCK_SIZE = 2;
constexpr uint32_t MAX_BLOCK_BYTES = BASE_BLOCK_BYTES << MAX_LOG_BLOCK_SIZE;
constexpr uint32_t MAX_POINTERS_PER_BLOCK = MAX_BLOCK_BYTES / sizeof(uint32_t);
constexpr uint32_t LEGACY_INODE_BYTES = 128;
constexpr uint32_t MAX_INODE_BYTES = 512;
constexpr uint32_t LEGACY_FIRST_INODE = 11;
constexpr uint32_t BITS_PER_BYTE = 8;

constexpr uint32_t INCOMPAT_COMPRESSION = 0x00001;
constexpr uint32_t INCOMPAT_FILETYPE = 0x00002;
constexpr uint32_t INCOMPAT_RECOVER = 0x00004;
constexpr uint32_t INCOMPAT_JOURNAL_DEVICE = 0x00008;
constexpr uint32_t INCOMPAT_META_BLOCK_GROUPS = 0x00010;
constexpr uint32_t INCOMPAT_EXTENTS = 0x00040;
constexpr uint32_t INCOMPAT_64BIT = 0x00080;
constexpr uint32_t INCOMPAT_MULTIPLE_MOUNT_PROTECTION = 0x00100;
constexpr uint32_t INCOMPAT_FLEX_BLOCK_GROUPS = 0x00200;
constexpr uint32_t INCOMPAT_EXTENDED_ATTRIBUTE_INODES = 0x00400;
constexpr uint32_t INCOMPAT_DIRECTORY_DATA = 0x01000;
constexpr uint32_t INCOMPAT_CHECKSUM_SEED = 0x02000;
constexpr uint32_t INCOMPAT_LARGE_DIRECTORIES = 0x04000;
constexpr uint32_t INCOMPAT_INLINE_DATA = 0x08000;
constexpr uint32_t INCOMPAT_ENCRYPTION = 0x10000;
constexpr uint32_t SUPPORTED_INCOMPAT = INCOMPAT_FILETYPE;
constexpr uint32_t COMPAT_HAS_JOURNAL = 0x00004;
constexpr uint32_t RO_COMPAT_SPARSE_SUPER = 0x00001;
constexpr uint32_t RO_COMPAT_LARGE_FILE = 0x00002;
constexpr uint32_t SUPPORTED_RO_COMPAT = RO_COMPAT_SPARSE_SUPER | RO_COMPAT_LARGE_FILE;
constexpr uint32_t STATE_VALID = 0x0001;
constexpr uint32_t STATE_ERROR = 0x0002;

constexpr size_t GROUP_DESCRIPTOR_BYTES = 32;
constexpr size_t GD_BLOCK_BITMAP_OFFSET = 0;
constexpr size_t GD_INODE_BITMAP_OFFSET = 4;
constexpr size_t GD_INODE_TABLE_OFFSET = 8;
constexpr size_t GD_FREE_BLOCKS_OFFSET = 12;
constexpr size_t GD_FREE_INODES_OFFSET = 14;

constexpr uint32_t ROOT_INODE = 2;
constexpr size_t INODE_MODE_OFFSET = 0;
constexpr size_t INODE_SIZE_OFFSET = 4;
constexpr size_t INODE_ACCESS_TIME_OFFSET = 8;
constexpr size_t INODE_CHANGE_TIME_OFFSET = 12;
constexpr size_t INODE_MODIFY_TIME_OFFSET = 16;
constexpr size_t INODE_DELETION_TIME_OFFSET = 20;
constexpr uint32_t DELETION_TIME_MARK = 1000000000;
constexpr size_t INODE_LINKS_OFFSET = 26;
constexpr size_t INODE_SECTORS_OFFSET = 28;
constexpr size_t INODE_FLAGS_OFFSET = 32;
constexpr size_t INODE_BLOCKS_OFFSET = 40;
constexpr size_t INODE_SIZE_HIGH_OFFSET = 108;
constexpr uint32_t INODE_BLOCK_SLOTS = 15;
constexpr uint32_t DIRECT_BLOCKS = 12;
constexpr uint32_t SINGLE_INDIRECT_SLOT = 12;
constexpr uint32_t DOUBLE_INDIRECT_SLOT = 13;
constexpr uint32_t TRIPLE_INDIRECT_SLOT = 14;
constexpr uint32_t INODE_EXTENTS_FLAG = 0x00080000;
constexpr uint32_t INODE_INLINE_DATA_FLAG = 0x10000000;
constexpr uint32_t MODE_TYPE_MASK = 0xF000;
constexpr uint32_t MODE_DIRECTORY = 0x4000;
constexpr uint32_t MODE_REGULAR = 0x8000;
constexpr uint32_t MODE_SYMLINK = 0xA000;
constexpr uint32_t MODE_PERMISSIONS_644 = 0x01A4;
constexpr uint32_t MODE_NEW_FILE = MODE_REGULAR | MODE_PERMISSIONS_644;
constexpr uint32_t NEW_FILE_LINKS = 1;
constexpr uint32_t MODE_PERMISSIONS_755 = 0x01ED;
constexpr uint32_t MODE_NEW_DIRECTORY = MODE_DIRECTORY | MODE_PERMISSIONS_755;
constexpr uint32_t NEW_DIRECTORY_LINKS = 2;
constexpr size_t GD_USED_DIRECTORIES_OFFSET = 16;
constexpr uint8_t DIRENT_TYPE_DIRECTORY = 2;
constexpr uint8_t DIRENT_TYPE_SYMLINK = 7;
constexpr char CURRENT_ENTRY_NAME[] = ".";
constexpr char PARENT_ENTRY_NAME[] = "..";
constexpr uint32_t CURRENT_ENTRY_BYTES = 1;
constexpr uint32_t PARENT_ENTRY_BYTES = 2;
constexpr uint32_t MAX_FILE_BYTES = 0x7FFFFFFF;
constexpr uint32_t FAST_SYMLINK_BYTES = INODE_BLOCK_SLOTS * sizeof(uint32_t);

constexpr size_t DIRENT_HEADER_BYTES = 8;
constexpr size_t DIRENT_RECORD_OFFSET = 4;
constexpr size_t DIRENT_NAME_LENGTH_OFFSET = 6;
constexpr size_t DIRENT_TYPE_OFFSET = 7;
constexpr size_t DIRENT_NAME_OFFSET = 8;
constexpr uint32_t DIRENT_ALIGNMENT = 4;
constexpr uint8_t DIRENT_TYPE_REGULAR = 1;
constexpr uint32_t NO_BIT = 0xFFFFFFFF;
constexpr uint8_t FULL_BYTE = 0xFF;
constexpr uint32_t FIRST_BIT = 1;
constexpr uint32_t BITS_PER_BITMAP_SECTOR = uint32_t(SECTOR_BYTES) * BITS_PER_BYTE;

constexpr unsigned SHIFT_8 = 8;
constexpr unsigned SHIFT_16 = 16;
constexpr unsigned SHIFT_24 = 24;
constexpr size_t THIRD_BYTE = 2;
constexpr size_t FOURTH_BYTE = 3;

enum class Error : uint32_t {
	NONE = 0,
	BAD_MAGIC,
	BAD_BLOCK_SIZE,
	BAD_GEOMETRY,
	BAD_INODE_SIZE,
	UNSUPPORTED_FEATURES,
};

enum class InodeType : uint32_t {
	OTHER = 0,
	REGULAR,
	DIRECTORY,
	SYMLINK,
};

struct Superblock {
	uint32_t inodesCount;
	uint32_t blocksCount;
	uint32_t firstDataBlock;
	uint32_t blockBytes;
	uint32_t blocksPerGroup;
	uint32_t inodesPerGroup;
	uint32_t inodeBytes;
	uint32_t groupCount;
	uint32_t compat;
	uint32_t incompat;
	uint32_t roCompat;
	uint32_t unsupported;
	uint32_t freeBlocks;
	uint32_t freeInodes;
	uint32_t state;
	bool fileType;
	bool writable;

	uint32_t sectorsPerBlock() const { return blockBytes / uint32_t(SECTOR_BYTES); }
	uint32_t pointersPerBlock() const { return blockBytes / uint32_t(sizeof(uint32_t)); }
	uint32_t groupTableSector() const { return (firstDataBlock + 1) * sectorsPerBlock(); }
};

struct Inode {
	InodeType type;
	uint32_t size;
	uint32_t links;
	uint32_t sectors;
	uint32_t flags;
	uint32_t blocks[INODE_BLOCK_SLOTS];
};

struct DirItem {
	uint32_t inode;
	uint32_t recordBytes;
	uint32_t nameBytes;
	const uint8_t *name;
};

struct BlockPath {
	uint32_t depth;
	uint32_t slots[4];
};

struct FeatureName {
	uint32_t bit;
	const char *name;
};

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
constexpr size_t INCOMPAT_NAME_COUNT = sizeof(INCOMPAT_NAMES) / sizeof(INCOMPAT_NAMES[0]);
constexpr char UNKNOWN_FEATURE_NAME[] = "unknown";

namespace detail {

inline uint16_t readLe16(const uint8_t *bytes) {
	return uint16_t(uint16_t(bytes[0]) | uint16_t(uint16_t(bytes[1]) << SHIFT_8));
}

inline uint32_t readLe32(const uint8_t *bytes) {
	return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << SHIFT_8) |
		   (uint32_t(bytes[THIRD_BYTE]) << SHIFT_16) |
		   (uint32_t(bytes[FOURTH_BYTE]) << SHIFT_24);
}

inline bool isPowerOfTwo(uint32_t value) { return value != 0 && (value & (value - 1)) == 0; }

inline void writeLe16(uint8_t *bytes, uint32_t value) {
	bytes[0] = uint8_t(value);
	bytes[1] = uint8_t(value >> SHIFT_8);
}

inline void writeLe32(uint8_t *bytes, uint32_t value) {
	bytes[0] = uint8_t(value);
	bytes[1] = uint8_t(value >> SHIFT_8);
	bytes[THIRD_BYTE] = uint8_t(value >> SHIFT_16);
	bytes[FOURTH_BYTE] = uint8_t(value >> SHIFT_24);
}

} // namespace detail

inline const char *featureName(uint32_t bit) {
	for (size_t index = 0; index < INCOMPAT_NAME_COUNT; index++)
		if (INCOMPAT_NAMES[index].bit == bit) return INCOMPAT_NAMES[index].name;
	return UNKNOWN_FEATURE_NAME;
}

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

inline uint32_t direntBytes(uint32_t nameBytes) {
	return (uint32_t(DIRENT_HEADER_BYTES) + nameBytes + DIRENT_ALIGNMENT - 1) /
		   DIRENT_ALIGNMENT * DIRENT_ALIGNMENT;
}

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

inline uint8_t bitMask(uint32_t index) { return uint8_t(FIRST_BIT << (index % BITS_PER_BYTE)); }

inline bool bitIsSet(const uint8_t *bitmap, uint32_t index) {
	return (bitmap[index / BITS_PER_BYTE] & bitMask(index)) != 0;
}

inline void setBit(uint8_t *bitmap, uint32_t index) {
	bitmap[index / BITS_PER_BYTE] = uint8_t(bitmap[index / BITS_PER_BYTE] | bitMask(index));
}

inline void clearBit(uint8_t *bitmap, uint32_t index) {
	bitmap[index / BITS_PER_BYTE] = uint8_t(bitmap[index / BITS_PER_BYTE] & ~bitMask(index));
}

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
