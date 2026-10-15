#include <cock/abi/ext2.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using namespace cock::abi::ext2;

namespace {

constexpr uint64_t PARTITION_SECTORS = 20480;
constexpr uint32_t BLOCKS_COUNT = 10240;
constexpr uint32_t INODES_COUNT = 2560;
constexpr uint32_t BLOCKS_PER_GROUP = 8192;
constexpr uint32_t INODES_PER_GROUP = 1280;
constexpr uint32_t EXPECTED_GROUPS = 2;
constexpr uint32_t FIRST_DATA_BLOCK = 1;
constexpr uint32_t DYNAMIC_INODE_BYTES = 256;
constexpr uint32_t WRONG_INODE_BYTES = 100;
constexpr uint32_t HUGE_INODE_BYTES = 1024;
constexpr uint32_t COMPAT_FLAGS = 0x38;
constexpr uint32_t RO_COMPAT_FLAGS = 0x3;
constexpr uint32_t EXTENT_FEATURES = INCOMPAT_FILETYPE | INCOMPAT_EXTENTS | INCOMPAT_FLEX_BLOCK_GROUPS;
constexpr uint32_t UNKNOWN_BIT = 0x40000000;
constexpr uint32_t LOG_FOUR_KIB = 2;
constexpr uint32_t LOG_TOO_BIG = 3;
constexpr uint32_t FOUR_KIB_BLOCK_BYTES = 4096;
constexpr uint32_t FOUR_KIB_BLOCKS = 2560;
constexpr uint32_t DIRECTORY_MODE_BITS = 0x41ED;
constexpr uint32_t REGULAR_MODE_BITS = 0x81A4;
constexpr uint32_t SYMLINK_MODE_BITS = 0xA1FF;
constexpr uint32_t DEVICE_MODE_BITS = 0x61B0;
constexpr uint32_t FILE_BYTES = 123456;
constexpr uint32_t HIGH_SIZE = 1;
constexpr uint32_t FLAGS_VALUE = 0x1000;
constexpr uint32_t BLOCK_POINTER_BASE = 100;
constexpr uint32_t BLOCK_BYTES = 1024;
constexpr uint32_t POINTERS = 256;
constexpr uint32_t FIRST_ENTRY_INODE = 12;
constexpr uint32_t FIRST_ENTRY_RECORD = 16;
constexpr uint32_t SECOND_ENTRY_INODE = 13;
constexpr uint32_t ENTRY_NAME_BYTES = 5;
constexpr uint32_t LAST_RECORD = 1008;
constexpr uint32_t MISALIGNED_RECORD = 18;
constexpr uint32_t SHORT_RECORD = 4;
constexpr uint32_t BEYOND_RECORD = 2048;
constexpr uint32_t TOO_LONG_NAME = 250;
constexpr uint32_t WIDE_NAME_BYTES = 300;
constexpr uint32_t WIDE_RECORD = 316;
constexpr uint32_t TRIPLE_START = DIRECT_BLOCKS + POINTERS + POINTERS * POINTERS;
constexpr uint32_t TRIPLE_END = TRIPLE_START + POINTERS * POINTERS * POINTERS;
constexpr uint32_t SECOND_LEVEL_SLOT = 3;
constexpr uint32_t THIRD_LEVEL_SLOT = 7;
constexpr uint32_t LAST_SLOT = 255;
constexpr uint32_t BYTE_BITS = 8;
constexpr uint32_t WORD_BYTES = 4;

void putLe16(uint8_t *at, uint32_t value) {
	at[0] = static_cast<uint8_t>(value);
	at[1] = static_cast<uint8_t>(value >> BYTE_BITS);
}

void putLe32(uint8_t *at, uint32_t value) {
	for (uint32_t index = 0; index < WORD_BYTES; index++)
		at[index] = static_cast<uint8_t>(value >> (index * BYTE_BITS));
}

void makeSuperblock(uint8_t *bytes) {
	memset(bytes, 0, SUPERBLOCK_BYTES);
	putLe32(bytes + SB_INODES_COUNT_OFFSET, INODES_COUNT);
	putLe32(bytes + SB_BLOCKS_COUNT_OFFSET, BLOCKS_COUNT);
	putLe32(bytes + SB_FIRST_DATA_BLOCK_OFFSET, FIRST_DATA_BLOCK);
	putLe32(bytes + SB_LOG_BLOCK_SIZE_OFFSET, 0);
	putLe32(bytes + SB_BLOCKS_PER_GROUP_OFFSET, BLOCKS_PER_GROUP);
	putLe32(bytes + SB_INODES_PER_GROUP_OFFSET, INODES_PER_GROUP);
	putLe16(bytes + SB_MAGIC_OFFSET, MAGIC);
	putLe32(bytes + SB_REVISION_OFFSET, REVISION_DYNAMIC);
	putLe16(bytes + SB_INODE_SIZE_OFFSET, DYNAMIC_INODE_BYTES);
	putLe32(bytes + SB_COMPAT_OFFSET, COMPAT_FLAGS);
	putLe32(bytes + SB_INCOMPAT_OFFSET, INCOMPAT_FILETYPE);
	putLe32(bytes + SB_RO_COMPAT_OFFSET, RO_COMPAT_FLAGS);
}

void checkSuperblock() {
	uint8_t bytes[SUPERBLOCK_BYTES];
	Superblock sb = {};
	makeSuperblock(bytes);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_EQ(BASE_BLOCK_BYTES, sb.blockBytes);
	CHECK_EQ(DYNAMIC_INODE_BYTES, sb.inodeBytes);
	CHECK_EQ(EXPECTED_GROUPS, sb.groupCount);
	CHECK_TRUE(sb.fileType);
	CHECK_EQ(uint32_t(BLOCK_BYTES / SECTOR_BYTES), sb.sectorsPerBlock());
	CHECK_EQ(POINTERS, sb.pointersPerBlock());
	CHECK_EQ((FIRST_DATA_BLOCK + 1) * sb.sectorsPerBlock(), sb.groupTableSector());

	makeSuperblock(bytes);
	putLe32(bytes + SB_REVISION_OFFSET, 0);
	putLe32(bytes + SB_INCOMPAT_OFFSET, INCOMPAT_EXTENTS);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_EQ(LEGACY_INODE_BYTES, sb.inodeBytes);
	CHECK_FALSE(sb.fileType);

	makeSuperblock(bytes);
	putLe32(bytes + SB_LOG_BLOCK_SIZE_OFFSET, LOG_FOUR_KIB);
	putLe32(bytes + SB_FIRST_DATA_BLOCK_OFFSET, 0);
	putLe32(bytes + SB_BLOCKS_COUNT_OFFSET, FOUR_KIB_BLOCKS);
	putLe32(bytes + SB_INODES_COUNT_OFFSET, INODES_PER_GROUP);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_EQ(FOUR_KIB_BLOCK_BYTES, sb.blockBytes);
}

void checkSuperblockRejections() {
	uint8_t bytes[SUPERBLOCK_BYTES];
	Superblock sb = {};

	makeSuperblock(bytes);
	putLe16(bytes + SB_MAGIC_OFFSET, 0);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::BAD_MAGIC);

	makeSuperblock(bytes);
	putLe32(bytes + SB_LOG_BLOCK_SIZE_OFFSET, LOG_TOO_BIG);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::BAD_BLOCK_SIZE);

	makeSuperblock(bytes);
	putLe32(bytes + SB_INCOMPAT_OFFSET, EXTENT_FEATURES);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::UNSUPPORTED_FEATURES);
	CHECK_EQ(INCOMPAT_EXTENTS | INCOMPAT_FLEX_BLOCK_GROUPS, sb.unsupported);
	CHECK_TRUE(strcmp(featureName(INCOMPAT_EXTENTS), "extents") == 0);
	CHECK_TRUE(strcmp(featureName(INCOMPAT_FLEX_BLOCK_GROUPS), "flex_bg") == 0);
	CHECK_TRUE(strcmp(featureName(UNKNOWN_BIT), UNKNOWN_FEATURE_NAME) == 0);

	makeSuperblock(bytes);
	putLe32(bytes + SB_INCOMPAT_OFFSET, INCOMPAT_FILETYPE | INCOMPAT_RECOVER);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::UNSUPPORTED_FEATURES);
	CHECK_EQ(INCOMPAT_RECOVER, sb.unsupported);

	makeSuperblock(bytes);
	putLe32(bytes + SB_INCOMPAT_OFFSET, UNKNOWN_BIT);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::UNSUPPORTED_FEATURES);

	makeSuperblock(bytes);
	putLe16(bytes + SB_INODE_SIZE_OFFSET, WRONG_INODE_BYTES);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::BAD_INODE_SIZE);
	putLe16(bytes + SB_INODE_SIZE_OFFSET, HUGE_INODE_BYTES);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::BAD_INODE_SIZE);

	makeSuperblock(bytes);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS - 1, sb) == Error::BAD_GEOMETRY);

	makeSuperblock(bytes);
	putLe32(bytes + SB_FIRST_DATA_BLOCK_OFFSET, 0);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::BAD_GEOMETRY);

	makeSuperblock(bytes);
	putLe32(bytes + SB_BLOCKS_PER_GROUP_OFFSET, 0);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::BAD_GEOMETRY);

	makeSuperblock(bytes);
	putLe32(bytes + SB_INODES_COUNT_OFFSET, INODES_COUNT * HIGH_SIZE * EXPECTED_GROUPS);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::BAD_GEOMETRY);
}

void makeInode(uint8_t *raw, uint32_t mode) {
	memset(raw, 0, LEGACY_INODE_BYTES);
	putLe16(raw + INODE_MODE_OFFSET, mode);
	putLe32(raw + INODE_SIZE_OFFSET, FILE_BYTES);
	putLe32(raw + INODE_FLAGS_OFFSET, FLAGS_VALUE);
	for (uint32_t slot = 0; slot < INODE_BLOCK_SLOTS; slot++)
		putLe32(raw + INODE_BLOCKS_OFFSET + slot * WORD_BYTES, BLOCK_POINTER_BASE + slot);
}

void checkInodes() {
	uint8_t raw[LEGACY_INODE_BYTES];

	makeInode(raw, REGULAR_MODE_BITS);
	Inode inode = parseInode(raw);
	CHECK_TRUE(inode.type == InodeType::REGULAR);
	CHECK_EQ(FILE_BYTES, inode.size);
	CHECK_EQ(FLAGS_VALUE, inode.flags);
	CHECK_EQ(BLOCK_POINTER_BASE, inode.blocks[0]);
	CHECK_EQ(BLOCK_POINTER_BASE + TRIPLE_INDIRECT_SLOT, inode.blocks[TRIPLE_INDIRECT_SLOT]);

	makeInode(raw, DIRECTORY_MODE_BITS);
	CHECK_TRUE(parseInode(raw).type == InodeType::DIRECTORY);
	makeInode(raw, SYMLINK_MODE_BITS);
	CHECK_TRUE(parseInode(raw).type == InodeType::SYMLINK);
	makeInode(raw, DEVICE_MODE_BITS);
	CHECK_TRUE(parseInode(raw).type == InodeType::OTHER);

	makeInode(raw, REGULAR_MODE_BITS);
	putLe32(raw + INODE_SIZE_HIGH_OFFSET, HIGH_SIZE);
	CHECK_EQ(MAX_FILE_BYTES, parseInode(raw).size);

	makeInode(raw, DIRECTORY_MODE_BITS);
	putLe32(raw + INODE_SIZE_HIGH_OFFSET, HIGH_SIZE);
	CHECK_EQ(FILE_BYTES, parseInode(raw).size);
}

void putEntry(uint8_t *block, uint32_t offset, uint32_t inode, uint32_t record,
			  uint32_t nameBytes, bool wide) {
	uint8_t *raw = block + offset;
	putLe32(raw, inode);
	putLe16(raw + DIRENT_RECORD_OFFSET, record);
	if (wide) {
		putLe16(raw + DIRENT_NAME_LENGTH_OFFSET, nameBytes);
	} else {
		raw[DIRENT_NAME_LENGTH_OFFSET] = static_cast<uint8_t>(nameBytes);
	}
}

void checkDirectoryItems() {
	uint8_t block[BLOCK_BYTES] = {};
	DirItem item = {};

	putEntry(block, 0, FIRST_ENTRY_INODE, FIRST_ENTRY_RECORD, ENTRY_NAME_BYTES, false);
	memcpy(block + DIRENT_NAME_OFFSET, "hello", ENTRY_NAME_BYTES);
	putEntry(block, FIRST_ENTRY_RECORD, SECOND_ENTRY_INODE, LAST_RECORD, 0, false);
	CHECK_TRUE(parseDirItem(block, BLOCK_BYTES, 0, true, item));
	CHECK_EQ(FIRST_ENTRY_INODE, item.inode);
	CHECK_EQ(FIRST_ENTRY_RECORD, item.recordBytes);
	CHECK_EQ(ENTRY_NAME_BYTES, item.nameBytes);
	CHECK_TRUE(memcmp(item.name, "hello", ENTRY_NAME_BYTES) == 0);
	CHECK_TRUE(parseDirItem(block, BLOCK_BYTES, FIRST_ENTRY_RECORD, true, item));
	CHECK_EQ(SECOND_ENTRY_INODE, item.inode);
	CHECK_EQ(LAST_RECORD, item.recordBytes);

	putEntry(block, 0, FIRST_ENTRY_INODE, SHORT_RECORD, 0, false);
	CHECK_FALSE(parseDirItem(block, BLOCK_BYTES, 0, true, item));
	putEntry(block, 0, FIRST_ENTRY_INODE, MISALIGNED_RECORD, 0, false);
	CHECK_FALSE(parseDirItem(block, BLOCK_BYTES, 0, true, item));
	putEntry(block, 0, FIRST_ENTRY_INODE, BEYOND_RECORD, 0, false);
	CHECK_FALSE(parseDirItem(block, BLOCK_BYTES, 0, true, item));
	putEntry(block, 0, FIRST_ENTRY_INODE, FIRST_ENTRY_RECORD, TOO_LONG_NAME, false);
	CHECK_FALSE(parseDirItem(block, BLOCK_BYTES, 0, true, item));
	CHECK_FALSE(parseDirItem(block, BLOCK_BYTES, BLOCK_BYTES, true, item));
	CHECK_FALSE(parseDirItem(block, BLOCK_BYTES, BLOCK_BYTES - DIRENT_ALIGNMENT, true, item));

	putEntry(block, 0, FIRST_ENTRY_INODE, WIDE_RECORD, WIDE_NAME_BYTES, true);
	CHECK_TRUE(parseDirItem(block, BLOCK_BYTES, 0, false, item));
	CHECK_EQ(WIDE_NAME_BYTES, item.nameBytes);
}

void checkBlockPaths() {
	BlockPath path = {};

	CHECK_TRUE(blockPath(0, POINTERS, path));
	CHECK_EQ(0u, path.depth);
	CHECK_EQ(0u, path.slots[0]);
	CHECK_TRUE(blockPath(DIRECT_BLOCKS - 1, POINTERS, path));
	CHECK_EQ(0u, path.depth);
	CHECK_EQ(DIRECT_BLOCKS - 1, path.slots[0]);

	CHECK_TRUE(blockPath(DIRECT_BLOCKS, POINTERS, path));
	CHECK_EQ(1u, path.depth);
	CHECK_EQ(SINGLE_INDIRECT_SLOT, path.slots[0]);
	CHECK_EQ(0u, path.slots[1]);
	CHECK_TRUE(blockPath(DIRECT_BLOCKS + POINTERS - 1, POINTERS, path));
	CHECK_EQ(1u, path.depth);
	CHECK_EQ(LAST_SLOT, path.slots[1]);

	CHECK_TRUE(blockPath(DIRECT_BLOCKS + POINTERS, POINTERS, path));
	CHECK_EQ(2u, path.depth);
	CHECK_EQ(DOUBLE_INDIRECT_SLOT, path.slots[0]);
	CHECK_EQ(0u, path.slots[1]);
	CHECK_EQ(0u, path.slots[2]);
	CHECK_TRUE(blockPath(DIRECT_BLOCKS + POINTERS + SECOND_LEVEL_SLOT * POINTERS + THIRD_LEVEL_SLOT,
						 POINTERS, path));
	CHECK_EQ(SECOND_LEVEL_SLOT, path.slots[1]);
	CHECK_EQ(THIRD_LEVEL_SLOT, path.slots[2]);
	CHECK_TRUE(blockPath(TRIPLE_START - 1, POINTERS, path));
	CHECK_EQ(2u, path.depth);
	CHECK_EQ(LAST_SLOT, path.slots[1]);
	CHECK_EQ(LAST_SLOT, path.slots[2]);

	CHECK_TRUE(blockPath(TRIPLE_START, POINTERS, path));
	CHECK_EQ(3u, path.depth);
	CHECK_EQ(TRIPLE_INDIRECT_SLOT, path.slots[0]);
	CHECK_EQ(0u, path.slots[1]);
	CHECK_EQ(0u, path.slots[2]);
	CHECK_EQ(0u, path.slots[3]);
	CHECK_TRUE(blockPath(TRIPLE_END - 1, POINTERS, path));
	CHECK_EQ(LAST_SLOT, path.slots[1]);
	CHECK_EQ(LAST_SLOT, path.slots[2]);
	CHECK_EQ(LAST_SLOT, path.slots[3]);
	CHECK_FALSE(blockPath(TRIPLE_END, POINTERS, path));
	CHECK_FALSE(blockPath(0, 0, path));
	CHECK_FALSE(blockPath(0, MAX_POINTERS_PER_BLOCK + 1, path));
}

constexpr uint32_t FREE_BLOCKS = 9000;
constexpr uint32_t FREE_INODES = 2500;
constexpr uint32_t UNKNOWN_RO_COMPAT = 0x40;
constexpr uint32_t ENCODED_INODE = 21;
constexpr char ENCODED_NAME[] = "note.txt";
constexpr uint32_t ENCODED_NAME_BYTES = sizeof(ENCODED_NAME) - sizeof(char);
constexpr uint32_t ENCODED_RECORD_BYTES = 24;
constexpr uint32_t EXPECTED_ENCODED_SIZE = 16;
constexpr uint32_t ONE_BYTE_NAME_RECORD = 12;
constexpr uint32_t BITMAP_BYTES = 8;
constexpr uint32_t BITMAP_BITS = BITMAP_BYTES * BYTE_BITS;
constexpr uint32_t FIRST_CLEAR_BIT = 13;
constexpr uint32_t BITMAP_START = 14;
constexpr uint32_t BITMAP_LIMIT = 20;
constexpr uint32_t INODE_LINKS = 3;
constexpr uint32_t INODE_SECTOR_COUNT = 24;
constexpr uint32_t SECTOR_BITS = 4096;

void makeWritableSuperblock(uint8_t *bytes) {
	makeSuperblock(bytes);
	putLe32(bytes + SB_FREE_BLOCKS_OFFSET, FREE_BLOCKS);
	putLe32(bytes + SB_FREE_INODES_OFFSET, FREE_INODES);
	putLe16(bytes + SB_STATE_OFFSET, STATE_VALID);
}

void checkWritableVolumes() {
	uint8_t bytes[SUPERBLOCK_BYTES];
	Superblock sb = {};

	makeWritableSuperblock(bytes);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_TRUE(sb.writable);
	CHECK_EQ(FREE_BLOCKS, sb.freeBlocks);
	CHECK_EQ(FREE_INODES, sb.freeInodes);
	CHECK_EQ(uint32_t(STATE_VALID), sb.state);

	makeWritableSuperblock(bytes);
	putLe16(bytes + SB_STATE_OFFSET, 0);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_FALSE(sb.writable);

	makeWritableSuperblock(bytes);
	putLe16(bytes + SB_STATE_OFFSET, STATE_VALID | STATE_ERROR);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_FALSE(sb.writable);

	makeWritableSuperblock(bytes);
	putLe32(bytes + SB_COMPAT_OFFSET, COMPAT_FLAGS | COMPAT_HAS_JOURNAL);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_FALSE(sb.writable);

	makeWritableSuperblock(bytes);
	putLe32(bytes + SB_RO_COMPAT_OFFSET, RO_COMPAT_FLAGS | UNKNOWN_RO_COMPAT);
	CHECK_TRUE(parseSuperblock(bytes, PARTITION_SECTORS, sb) == Error::NONE);
	CHECK_FALSE(sb.writable);
}

void checkInodeFields() {
	uint8_t raw[LEGACY_INODE_BYTES] = {};
	putLe16(raw + INODE_MODE_OFFSET, MODE_NEW_FILE);
	putLe16(raw + INODE_LINKS_OFFSET, INODE_LINKS);
	putLe32(raw + INODE_SECTORS_OFFSET, INODE_SECTOR_COUNT);
	putLe32(raw + INODE_SIZE_OFFSET, FILE_BYTES);

	Inode inode = parseInode(raw);
	CHECK_TRUE(inode.type == InodeType::REGULAR);
	CHECK_EQ(INODE_LINKS, inode.links);
	CHECK_EQ(INODE_SECTOR_COUNT, inode.sectors);
	CHECK_EQ(FILE_BYTES, inode.size);
}

void checkDirectoryEncoding() {
	CHECK_EQ(EXPECTED_ENCODED_SIZE, direntBytes(ENCODED_NAME_BYTES));
	CHECK_EQ(ONE_BYTE_NAME_RECORD, direntBytes(1));
	CHECK_EQ(uint32_t(DIRENT_HEADER_BYTES), direntBytes(0));

	uint8_t block[BLOCK_BYTES] = {};
	DirItem item = {};
	encodeDirItem(block, ENCODED_INODE, ENCODED_RECORD_BYTES, ENCODED_NAME,
				  ENCODED_NAME_BYTES, true, DIRENT_TYPE_REGULAR);
	CHECK_TRUE(parseDirItem(block, BLOCK_BYTES, 0, true, item));
	CHECK_EQ(ENCODED_INODE, item.inode);
	CHECK_EQ(ENCODED_RECORD_BYTES, item.recordBytes);
	CHECK_EQ(ENCODED_NAME_BYTES, item.nameBytes);
	CHECK_TRUE(memcmp(item.name, ENCODED_NAME, ENCODED_NAME_BYTES) == 0);
	CHECK_EQ(uint8_t(DIRENT_TYPE_REGULAR), block[DIRENT_TYPE_OFFSET]);

	uint8_t plain[BLOCK_BYTES] = {};
	encodeDirItem(plain, ENCODED_INODE, EXPECTED_ENCODED_SIZE, ENCODED_NAME,
				  ENCODED_NAME_BYTES, false, DIRENT_TYPE_REGULAR);
	CHECK_TRUE(parseDirItem(plain, BLOCK_BYTES, 0, false, item));
	CHECK_EQ(ENCODED_NAME_BYTES, item.nameBytes);
	CHECK_EQ(uint8_t(0), plain[DIRENT_TYPE_OFFSET]);
}

void checkDirectoryBlock() {
	constexpr uint32_t SELF_INODE = 30;
	constexpr uint32_t PARENT_INODE = 2;
	constexpr uint32_t DOT_RECORD = 12;
	uint8_t block[BLOCK_BYTES];
	memset(block, FULL_BYTE, sizeof(block));
	initDirectoryBlock(block, BLOCK_BYTES, SELF_INODE, PARENT_INODE, true);

	DirItem item = {};
	CHECK_TRUE(parseDirItem(block, BLOCK_BYTES, 0, true, item));
	CHECK_EQ(SELF_INODE, item.inode);
	CHECK_EQ(DOT_RECORD, item.recordBytes);
	CHECK_EQ(CURRENT_ENTRY_BYTES, item.nameBytes);
	CHECK_TRUE(memcmp(item.name, CURRENT_ENTRY_NAME, item.nameBytes) == 0);
	CHECK_EQ(uint8_t(DIRENT_TYPE_DIRECTORY), block[DIRENT_TYPE_OFFSET]);

	CHECK_TRUE(parseDirItem(block, BLOCK_BYTES, DOT_RECORD, true, item));
	CHECK_EQ(PARENT_INODE, item.inode);
	CHECK_EQ(BLOCK_BYTES - DOT_RECORD, item.recordBytes);
	CHECK_EQ(PARENT_ENTRY_BYTES, item.nameBytes);
	CHECK_TRUE(memcmp(item.name, PARENT_ENTRY_NAME, item.nameBytes) == 0);
	CHECK_FALSE(parseDirItem(block, BLOCK_BYTES, BLOCK_BYTES, true, item));
}

void checkBitmaps() {
	uint8_t bitmap[BITMAP_BYTES];
	memset(bitmap, FULL_BYTE, sizeof(bitmap));
	CHECK_EQ(NO_BIT, findClearBit(bitmap, 0, BITMAP_BITS));

	clearBit(bitmap, FIRST_CLEAR_BIT);
	CHECK_FALSE(bitIsSet(bitmap, FIRST_CLEAR_BIT));
	CHECK_TRUE(bitIsSet(bitmap, FIRST_CLEAR_BIT - 1));
	CHECK_TRUE(bitIsSet(bitmap, FIRST_CLEAR_BIT + 1));
	CHECK_EQ(FIRST_CLEAR_BIT, findClearBit(bitmap, 0, BITMAP_BITS));
	CHECK_EQ(NO_BIT, findClearBit(bitmap, BITMAP_START, BITMAP_BITS));
	CHECK_EQ(NO_BIT, findClearBit(bitmap, 0, FIRST_CLEAR_BIT));

	setBit(bitmap, FIRST_CLEAR_BIT);
	CHECK_TRUE(bitIsSet(bitmap, FIRST_CLEAR_BIT));
	CHECK_EQ(NO_BIT, findClearBit(bitmap, 0, BITMAP_BITS));

	clearBit(bitmap, BITMAP_LIMIT);
	CHECK_EQ(BITMAP_LIMIT, findClearBit(bitmap, BITMAP_START, BITMAP_BITS));
	CHECK_EQ(NO_BIT, findClearBit(bitmap, BITMAP_START, BITMAP_LIMIT));
	CHECK_EQ(SECTOR_BITS, BITS_PER_BITMAP_SECTOR);
}

} // namespace

void testExt2() {
	checkSuperblock();
	checkSuperblockRejections();
	checkInodes();
	checkDirectoryItems();
	checkBlockPaths();
	checkWritableVolumes();
	checkInodeFields();
	checkDirectoryEncoding();
	checkDirectoryBlock();
	checkBitmaps();
}

} // namespace host_test
