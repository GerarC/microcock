#include <cock/abi/fat32.hpp>
#include <host_test/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using namespace cock::abi::fat32;

namespace {

constexpr uint32_t CLUSTER_SECTORS = 1;
constexpr uint32_t RESERVED_SECTORS = 32;
constexpr uint32_t FAT_COUNT = 2;
constexpr uint32_t FAT_SECTORS = 640;
constexpr uint32_t TOTAL_SECTORS = 81920;
constexpr uint32_t EXPECTED_CLUSTERS = TOTAL_SECTORS - RESERVED_SECTORS - FAT_COUNT * FAT_SECTORS;
constexpr uint32_t EXPECTED_DATA_START = RESERVED_SECTORS + FAT_COUNT * FAT_SECTORS;
constexpr uint32_t ROOT_CLUSTER = 2;
constexpr uint32_t SMALL_TOTAL = 4000;
constexpr uint32_t BAD_CLUSTER_SECTORS = 3;
constexpr uint32_t BIG_SECTOR_BYTES = 1024;
constexpr uint32_t BEYOND_DATA_CLUSTER = EXPECTED_CLUSTERS + FIRST_DATA_CLUSTER;
constexpr uint32_t FAT_RAW_VALUE = 0xF0000005;
constexpr uint32_t FAT_MASKED_VALUE = 0x00000005;
constexpr uint32_t FAT_CHAIN_END = 0x0FFFFFFF;
constexpr uint32_t FAT_ENTRY_INDEX = 3;
constexpr uint32_t FILE_CLUSTER = 0x00012345;
constexpr uint32_t FILE_SIZE = 20000;
constexpr uint8_t FILE_ATTRIBUTES = 0x20;
constexpr char LONG_NAME_TEXT[] = "A long file name.txt";
constexpr size_t LONG_NAME_LENGTH = sizeof(LONG_NAME_TEXT) - sizeof(char);
constexpr size_t SECOND_PART = 1;
constexpr uint8_t SECOND_PART_INDEX = 2;
constexpr uint8_t SECOND_PART_ORDER = LONG_LAST_FLAG | SECOND_PART_INDEX;
constexpr uint8_t FIRST_PART_ORDER = 1;
constexpr uint8_t WRONG_CHECKSUM_DELTA = 1;
constexpr uint8_t PADDING_LOW = 0xFF;
constexpr uint8_t HIGH_CLUSTER_SHIFT = 16;
constexpr uint8_t LOW_BYTE_MASK = 0xFF;
constexpr uint8_t BYTE_BITS = 8;
constexpr uint32_t NO_VALUE = 0;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr uint32_t LOW_HALF_MASK = 0xFFFF;

void putLe16(uint8_t *at, uint32_t value) {
	at[0] = static_cast<uint8_t>(value & LOW_BYTE_MASK);
	at[1] = static_cast<uint8_t>((value >> BYTE_BITS) & LOW_BYTE_MASK);
}

void putLe32(uint8_t *at, uint32_t value) {
	putLe16(at, value & LOW_HALF_MASK);
	putLe16(at + sizeof(uint16_t), value >> HIGH_CLUSTER_SHIFT);
}

void makeBpb(uint8_t *sector) {
	memset(sector, 0, SECTOR_BYTES);
	sector[BPB_SIGNATURE_OFFSET] = BPB_SIGNATURE_FIRST;
	sector[BPB_SIGNATURE_OFFSET + BPB_SIGNATURE_SECOND_STEP] = BPB_SIGNATURE_SECOND;
	putLe16(sector + BPB_SECTOR_BYTES_OFFSET, SECTOR_BYTES);
	sector[BPB_CLUSTER_SECTORS_OFFSET] = CLUSTER_SECTORS;
	putLe16(sector + BPB_RESERVED_OFFSET, RESERVED_SECTORS);
	sector[BPB_FAT_COUNT_OFFSET] = FAT_COUNT;
	putLe32(sector + BPB_TOTAL32_OFFSET, TOTAL_SECTORS);
	putLe32(sector + BPB_FAT_SIZE32_OFFSET, FAT_SECTORS);
	putLe32(sector + BPB_ROOT_CLUSTER_OFFSET, ROOT_CLUSTER);
}

void checkBpb() {
	uint8_t sector[SECTOR_BYTES];
	Bpb bpb = {};
	makeBpb(sector);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::NONE);
	CHECK_EQ(CLUSTER_SECTORS, bpb.sectorsPerCluster);
	CHECK_EQ(EXPECTED_CLUSTERS, bpb.clusterCount);
	CHECK_EQ(EXPECTED_DATA_START, bpb.dataStart);
	CHECK_EQ(ROOT_CLUSTER, bpb.rootCluster);
	CHECK_EQ(EXPECTED_DATA_START, bpb.clusterSector(ROOT_CLUSTER));
	CHECK_EQ(uint32_t(SECTOR_BYTES), bpb.clusterBytes());
	CHECK_TRUE(bpb.isDataCluster(ROOT_CLUSTER));
	CHECK_FALSE(bpb.isDataCluster(ROOT_CLUSTER - 1));
	CHECK_FALSE(bpb.isDataCluster(BEYOND_DATA_CLUSTER));
	CHECK_TRUE(bpb.isDataCluster(BEYOND_DATA_CLUSTER - 1));
}

void checkBpbRejections() {
	uint8_t sector[SECTOR_BYTES];
	Bpb bpb = {};

	makeBpb(sector);
	sector[BPB_SIGNATURE_OFFSET] = 0;
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::BAD_SIGNATURE);

	makeBpb(sector);
	putLe16(sector + BPB_SECTOR_BYTES_OFFSET, BIG_SECTOR_BYTES);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::BAD_SECTOR_SIZE);

	makeBpb(sector);
	sector[BPB_CLUSTER_SECTORS_OFFSET] = BAD_CLUSTER_SECTORS;
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::BAD_CLUSTER_SIZE);

	makeBpb(sector);
	putLe16(sector + BPB_FAT_SIZE16_OFFSET, SINGLE_SECTOR);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::NOT_FAT32);

	makeBpb(sector);
	putLe32(sector + BPB_TOTAL32_OFFSET, SMALL_TOTAL);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::NOT_FAT32);

	makeBpb(sector);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS - 1, bpb) == BpbError::BAD_LAYOUT);

	makeBpb(sector);
	putLe32(sector + BPB_ROOT_CLUSTER_OFFSET, NO_VALUE);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::BAD_LAYOUT);

	makeBpb(sector);
	putLe32(sector + BPB_ROOT_CLUSTER_OFFSET, BEYOND_DATA_CLUSTER);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::BAD_LAYOUT);

	makeBpb(sector);
	putLe32(sector + BPB_FAT_SIZE32_OFFSET, SINGLE_SECTOR);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::BAD_LAYOUT);

	makeBpb(sector);
	putLe16(sector + BPB_RESERVED_OFFSET, NO_VALUE);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::BAD_LAYOUT);
}

void checkFatValues() {
	uint8_t sector[SECTOR_BYTES] = {};
	putLe32(sector + FAT_ENTRY_INDEX * FAT_ENTRY_BYTES, FAT_RAW_VALUE);
	CHECK_EQ(FAT_MASKED_VALUE, fatValue(sector, FAT_ENTRY_INDEX));
	CHECK_FALSE(isEndOfChain(FAT_MASKED_VALUE));
	CHECK_TRUE(isEndOfChain(FAT_CHAIN_END));
	CHECK_TRUE(isEndOfChain(FAT_END_OF_CHAIN_MIN));
	CHECK_FALSE(isEndOfChain(FAT_BAD_CLUSTER));
}

void checkNames() {
	CHECK_TRUE(namesEqual("HELLO.TXT", "hello.txt"));
	CHECK_FALSE(namesEqual("HELLO.TXT", "hello.tx"));
	CHECK_FALSE(namesEqual("HELLO", "HELLO.TXT"));
	CHECK_TRUE(namesEqual("", ""));
}

void putShort(uint8_t *raw, const char *name11, uint8_t attributes, uint8_t caseFlags) {
	memset(raw, 0, ENTRY_BYTES);
	memcpy(raw + ENTRY_NAME_OFFSET, name11, ENTRY_SHORT_NAME_BYTES);
	raw[ENTRY_ATTRIBUTES_OFFSET] = attributes;
	raw[ENTRY_CASE_OFFSET] = caseFlags;
	putLe16(raw + ENTRY_CLUSTER_HIGH_OFFSET, FILE_CLUSTER >> HIGH_CLUSTER_SHIFT);
	putLe16(raw + ENTRY_CLUSTER_LOW_OFFSET, FILE_CLUSTER & LOW_HALF_MASK);
	putLe32(raw + ENTRY_SIZE_OFFSET, FILE_SIZE);
}

size_t longCharOffset(size_t slot) {
	if (slot < LONG_FIRST_GROUP_CHARS) return LONG_FIRST_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
	slot -= LONG_FIRST_GROUP_CHARS;
	if (slot < LONG_SECOND_GROUP_CHARS) return LONG_SECOND_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
	slot -= LONG_SECOND_GROUP_CHARS;
	return LONG_THIRD_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
}

void putLong(uint8_t *raw, uint8_t order, uint8_t checksum, const char *text, size_t length,
			 size_t start) {
	memset(raw, 0, ENTRY_BYTES);
	raw[ENTRY_NAME_OFFSET] = order;
	raw[ENTRY_ATTRIBUTES_OFFSET] = ATTRIBUTE_LONG_NAME;
	raw[LONG_CHECKSUM_OFFSET] = checksum;

	for (size_t slot = 0; slot < LONG_CHARS_PER_ENTRY; slot++) {
		size_t position = start + slot;
		uint8_t *target = raw + longCharOffset(slot);
		if (position < length) {
			putLe16(target, static_cast<uint8_t>(text[position]));
		} else if (position == length) {
			putLe16(target, LONG_NAME_END);
		} else {
			target[0] = PADDING_LOW;
			target[1] = PADDING_LOW;
		}
	}
}

void checkShortNames() {
	DirectoryParser parser;
	parser.reset();
	uint8_t raw[ENTRY_BYTES];
	DirEntry entry = {};

	putShort(raw, "HELLO   TXT", FILE_ATTRIBUTES, 0);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, "HELLO.TXT") == 0);
	CHECK_EQ(FILE_CLUSTER, entry.cluster);
	CHECK_EQ(FILE_SIZE, entry.size);
	CHECK_FALSE(entry.isDirectory());

	putShort(raw, "HELLO   TXT", FILE_ATTRIBUTES, CASE_LOWER_BASE | CASE_LOWER_EXTENSION);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, "hello.txt") == 0);

	putShort(raw, "BIG        ", FILE_ATTRIBUTES, 0);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, "BIG") == 0);

	putShort(raw, "SUBDIR     ", ATTRIBUTE_DIRECTORY, 0);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(entry.isDirectory());

	putShort(raw, "\x05" "BCD       ", FILE_ATTRIBUTES, 0);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_EQ(static_cast<unsigned char>(DIRECTORY_DELETED),
			 static_cast<unsigned char>(entry.name[0]));
}

void checkSkippedEntries() {
	DirectoryParser parser;
	parser.reset();
	uint8_t raw[ENTRY_BYTES];
	DirEntry entry = {};

	putShort(raw, ".          ", ATTRIBUTE_DIRECTORY, 0);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::SKIP);
	putShort(raw, "..         ", ATTRIBUTE_DIRECTORY, 0);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::SKIP);
	putShort(raw, "VOLUME     ", ATTRIBUTE_VOLUME_LABEL, 0);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::SKIP);

	putShort(raw, "GONE    TXT", FILE_ATTRIBUTES, 0);
	raw[ENTRY_NAME_OFFSET] = DIRECTORY_DELETED;
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::SKIP);

	memset(raw, 0, sizeof(raw));
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::END);
}

void checkLongNames() {
	DirectoryParser parser;
	parser.reset();
	uint8_t shortRaw[ENTRY_BYTES];
	uint8_t first[ENTRY_BYTES];
	uint8_t second[ENTRY_BYTES];
	DirEntry entry = {};

	putShort(shortRaw, "ALONGF~1TXT", FILE_ATTRIBUTES, 0);
	uint8_t checksum = shortNameChecksum(shortRaw);
	putLong(second, SECOND_PART_ORDER, checksum, LONG_NAME_TEXT, LONG_NAME_LENGTH,
			SECOND_PART * LONG_CHARS_PER_ENTRY);
	putLong(first, FIRST_PART_ORDER, checksum, LONG_NAME_TEXT, LONG_NAME_LENGTH, 0);

	CHECK_TRUE(parser.feed(second, entry) == DirectoryParser::Result::SKIP);
	CHECK_TRUE(parser.feed(first, entry) == DirectoryParser::Result::SKIP);
	CHECK_TRUE(parser.feed(shortRaw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, LONG_NAME_TEXT) == 0);

	putLong(second, SECOND_PART_ORDER, checksum + WRONG_CHECKSUM_DELTA, LONG_NAME_TEXT,
			LONG_NAME_LENGTH, SECOND_PART * LONG_CHARS_PER_ENTRY);
	putLong(first, FIRST_PART_ORDER, checksum + WRONG_CHECKSUM_DELTA, LONG_NAME_TEXT,
			LONG_NAME_LENGTH, 0);
	parser.feed(second, entry);
	parser.feed(first, entry);
	CHECK_TRUE(parser.feed(shortRaw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, "ALONGF~1.TXT") == 0);

	putLong(second, SECOND_PART_ORDER, checksum, LONG_NAME_TEXT, LONG_NAME_LENGTH,
			SECOND_PART * LONG_CHARS_PER_ENTRY);
	parser.feed(second, entry);
	CHECK_TRUE(parser.feed(shortRaw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, "ALONGF~1.TXT") == 0);

	putLong(first, FIRST_PART_ORDER, checksum, LONG_NAME_TEXT, LONG_NAME_LENGTH, 0);
	parser.feed(first, entry);
	CHECK_TRUE(parser.feed(shortRaw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, "ALONGF~1.TXT") == 0);

}

void putUnits(uint8_t *raw, uint8_t checksum, const uint16_t *units, size_t count) {
	memset(raw, 0, ENTRY_BYTES);
	raw[ENTRY_NAME_OFFSET] = LONG_LAST_FLAG | FIRST_PART_ORDER;
	raw[ENTRY_ATTRIBUTES_OFFSET] = ATTRIBUTE_LONG_NAME;
	raw[LONG_CHECKSUM_OFFSET] = checksum;
	for (size_t slot = 0; slot < LONG_CHARS_PER_ENTRY; slot++) {
		uint8_t *target = raw + longCharOffset(slot);
		if (slot < count) {
			putLe16(target, units[slot]);
		} else if (slot == count) {
			putLe16(target, LONG_NAME_END);
		} else {
			target[0] = PADDING_LOW;
			target[1] = PADDING_LOW;
		}
	}
}

bool namedAs(const uint16_t *units, size_t count, const char *expected) {
	DirectoryParser parser;
	parser.reset();
	uint8_t shortRaw[ENTRY_BYTES];
	uint8_t longRaw[ENTRY_BYTES];
	DirEntry entry = {};
	putShort(shortRaw, "NAME    TXT", FILE_ATTRIBUTES, 0);
	putUnits(longRaw, shortNameChecksum(shortRaw), units, count);
	parser.feed(longRaw, entry);
	return parser.feed(shortRaw, entry) == DirectoryParser::Result::ENTRY &&
		   strcmp(entry.name, expected) == 0;
}

void checkUnicodeNames() {
	const uint16_t accent[] = {0x00F1, 'a'};
	const uint16_t twoByte[] = {0x00E9};
	const uint16_t threeByte[] = {0x1234};
	const uint16_t pair[] = {0xD83D, 0xDE00};
	const uint16_t lonelyHigh[] = {0xD83D, 'a'};
	const uint16_t lonelyLow[] = {0xDE00};
	CHECK_TRUE(namedAs(accent, 2, "\xC3\xB1" "a"));
	CHECK_TRUE(namedAs(twoByte, 1, "\xC3\xA9"));
	CHECK_TRUE(namedAs(threeByte, 1, "\xE1\x88\xB4"));
	CHECK_TRUE(namedAs(pair, 2, "\xF0\x9F\x98\x80"));
	CHECK_TRUE(namedAs(lonelyHigh, 2, "?a"));
	CHECK_TRUE(namedAs(lonelyLow, 1, "?"));
}

constexpr uint32_t FAT_SET_VALUE = 0x00054321;
constexpr uint32_t FAT_FLAG_NIBBLE = 0xA0000000;
constexpr uint32_t FAT_NEIGHBOR_INDEX = 4;
constexpr uint32_t FAT_NEIGHBOR_VALUE = 0x0000BEEF;
constexpr uint32_t FSINFO_FREE_CLUSTERS = 12345;
constexpr uint32_t FSINFO_NEXT_CLUSTER = 77;

bool encodes(const char *name, uint8_t *shortName, uint8_t &caseFlags) {
	return encodeShortName(name, strlen(name), shortName, caseFlags);
}

void checkShortNameEncoding() {
	uint8_t shortName[ENTRY_SHORT_NAME_BYTES];
	uint8_t caseFlags = 0;

	CHECK_TRUE(encodes("HELLO.TXT", shortName, caseFlags));
	CHECK_TRUE(memcmp(shortName, "HELLO   TXT", ENTRY_SHORT_NAME_BYTES) == 0);
	CHECK_EQ(uint8_t(0), caseFlags);

	CHECK_TRUE(encodes("note.txt", shortName, caseFlags));
	CHECK_TRUE(memcmp(shortName, "NOTE    TXT", ENTRY_SHORT_NAME_BYTES) == 0);
	CHECK_EQ(uint8_t(CASE_LOWER_BASE | CASE_LOWER_EXTENSION), caseFlags);

	CHECK_TRUE(encodes("NOTE.txt", shortName, caseFlags));
	CHECK_EQ(uint8_t(CASE_LOWER_EXTENSION), caseFlags);

	CHECK_TRUE(encodes("readme", shortName, caseFlags));
	CHECK_TRUE(memcmp(shortName, "README     ", ENTRY_SHORT_NAME_BYTES) == 0);
	CHECK_EQ(uint8_t(CASE_LOWER_BASE), caseFlags);

	CHECK_TRUE(encodes("12345678.123", shortName, caseFlags));
	CHECK_TRUE(encodes("a_b-c.d1", shortName, caseFlags));
	CHECK_EQ(uint8_t(CASE_LOWER_BASE | CASE_LOWER_EXTENSION), caseFlags);

	CHECK_FALSE(encodes("", shortName, caseFlags));
	CHECK_FALSE(encodes(".", shortName, caseFlags));
	CHECK_FALSE(encodes("..", shortName, caseFlags));
	CHECK_FALSE(encodes(".profile", shortName, caseFlags));
	CHECK_FALSE(encodes("trailing.", shortName, caseFlags));
	CHECK_FALSE(encodes("MixedCase.txt", shortName, caseFlags));
	CHECK_FALSE(encodes("name.TxT", shortName, caseFlags));
	CHECK_FALSE(encodes("123456789.txt", shortName, caseFlags));
	CHECK_FALSE(encodes("name.text", shortName, caseFlags));
	CHECK_FALSE(encodes("a.b.c", shortName, caseFlags));
	CHECK_FALSE(encodes("with space.txt", shortName, caseFlags));
	CHECK_FALSE(encodes("star*.txt", shortName, caseFlags));
	CHECK_FALSE(encodes("caf\xC3\xA9.txt", shortName, caseFlags));
}

void checkEntryEncoding() {
	uint8_t shortName[ENTRY_SHORT_NAME_BYTES];
	uint8_t caseFlags = 0;
	uint8_t raw[ENTRY_BYTES];
	DirectoryParser parser;
	parser.reset();
	DirEntry entry = {};

	CHECK_TRUE(encodes("note.txt", shortName, caseFlags));
	encodeEntry(raw, shortName, caseFlags, ATTRIBUTE_ARCHIVE, NO_VALUE, NO_VALUE);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_TRUE(strcmp(entry.name, "note.txt") == 0);
	CHECK_EQ(NO_VALUE, entry.cluster);
	CHECK_EQ(NO_VALUE, entry.size);
	CHECK_FALSE(entry.isDirectory());
	CHECK_EQ(uint16_t(FAT_EPOCH_DATE), detail::readLe16(raw + ENTRY_WRITE_DATE_OFFSET));

	patchEntryLocation(raw, FILE_CLUSTER, FILE_SIZE);
	CHECK_TRUE(parser.feed(raw, entry) == DirectoryParser::Result::ENTRY);
	CHECK_EQ(FILE_CLUSTER, entry.cluster);
	CHECK_EQ(FILE_SIZE, entry.size);
	CHECK_TRUE(strcmp(entry.name, "note.txt") == 0);
}

void checkFatUpdates() {
	uint8_t sector[SECTOR_BYTES] = {};
	putLe32(sector + FAT_ENTRY_INDEX * FAT_ENTRY_BYTES, FAT_FLAG_NIBBLE | FAT_MASKED_VALUE);
	putLe32(sector + FAT_NEIGHBOR_INDEX * FAT_ENTRY_BYTES, FAT_NEIGHBOR_VALUE);

	setFatValue(sector, FAT_ENTRY_INDEX, FAT_SET_VALUE);
	CHECK_EQ(FAT_SET_VALUE, fatValue(sector, FAT_ENTRY_INDEX));
	CHECK_EQ(FAT_FLAG_NIBBLE, (detail::readLe32(sector + FAT_ENTRY_INDEX * FAT_ENTRY_BYTES) &
							   FAT_RESERVED_MASK));
	CHECK_EQ(FAT_NEIGHBOR_VALUE, fatValue(sector, FAT_NEIGHBOR_INDEX));

	setFatValue(sector, FAT_ENTRY_INDEX, FAT_FREE);
	CHECK_EQ(FAT_FREE, fatValue(sector, FAT_ENTRY_INDEX));
	setFatValue(sector, FAT_ENTRY_INDEX, FAT_END_OF_CHAIN);
	CHECK_TRUE(isEndOfChain(fatValue(sector, FAT_ENTRY_INDEX)));
}

void checkFsInfo() {
	uint8_t sector[SECTOR_BYTES] = {};
	CHECK_FALSE(validFsInfo(sector));

	putLe32(sector + FSINFO_LEAD_OFFSET, FSINFO_LEAD_SIGNATURE);
	putLe32(sector + FSINFO_STRUCT_OFFSET, FSINFO_STRUCT_SIGNATURE);
	CHECK_FALSE(validFsInfo(sector));
	putLe32(sector + FSINFO_TRAIL_OFFSET, FSINFO_TRAIL_SIGNATURE);
	CHECK_TRUE(validFsInfo(sector));

	storeFsInfo(sector, FSINFO_FREE_CLUSTERS, FSINFO_NEXT_CLUSTER);
	CHECK_EQ(FSINFO_FREE_CLUSTERS, fsInfoFree(sector));
	CHECK_EQ(FSINFO_NEXT_CLUSTER, fsInfoNext(sector));
	CHECK_TRUE(validFsInfo(sector));

	storeFsInfo(sector, FSINFO_UNKNOWN, FSINFO_UNKNOWN);
	CHECK_EQ(FSINFO_UNKNOWN, fsInfoFree(sector));
}

void checkBpbWriteFields() {
	uint8_t sector[SECTOR_BYTES];
	Bpb bpb = {};
	makeBpb(sector);
	putLe16(sector + BPB_FSINFO_OFFSET, SINGLE_SECTOR);
	CHECK_TRUE(parseBpb(sector, TOTAL_SECTORS, bpb) == BpbError::NONE);
	CHECK_EQ(FAT_COUNT, bpb.fatCount);
	CHECK_EQ(FAT_SECTORS, bpb.fatSectors);
	CHECK_EQ(SINGLE_SECTOR, bpb.fsInfoSector);
}

} // namespace

void testFat32() {
	checkBpb();
	checkBpbRejections();
	checkFatValues();
	checkNames();
	checkShortNames();
	checkSkippedEntries();
	checkLongNames();
	checkUnicodeNames();
	checkShortNameEncoding();
	checkEntryEncoding();
	checkFatUpdates();
	checkFsInfo();
	checkBpbWriteFields();
}

} // namespace host_test
