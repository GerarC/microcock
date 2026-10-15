#ifndef SHARED_COCK_ABI_FAT32_HPP
#define SHARED_COCK_ABI_FAT32_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::fat32 {

constexpr size_t SECTOR_BYTES = 512;
constexpr size_t ENTRY_BYTES = 32;
constexpr size_t ENTRIES_PER_SECTOR = SECTOR_BYTES / ENTRY_BYTES;
constexpr size_t FAT_ENTRY_BYTES = 4;
constexpr size_t FAT_ENTRIES_PER_SECTOR = SECTOR_BYTES / FAT_ENTRY_BYTES;

constexpr size_t BPB_SECTOR_BYTES_OFFSET = 11;
constexpr size_t BPB_CLUSTER_SECTORS_OFFSET = 13;
constexpr size_t BPB_RESERVED_OFFSET = 14;
constexpr size_t BPB_FAT_COUNT_OFFSET = 16;
constexpr size_t BPB_ROOT_ENTRIES_OFFSET = 17;
constexpr size_t BPB_TOTAL16_OFFSET = 19;
constexpr size_t BPB_FAT_SIZE16_OFFSET = 22;
constexpr size_t BPB_TOTAL32_OFFSET = 32;
constexpr size_t BPB_FAT_SIZE32_OFFSET = 36;
constexpr size_t BPB_VERSION_OFFSET = 42;
constexpr size_t BPB_ROOT_CLUSTER_OFFSET = 44;
constexpr size_t BPB_FSINFO_OFFSET = 48;
constexpr size_t BPB_SIGNATURE_OFFSET = 510;
constexpr uint8_t BPB_SIGNATURE_FIRST = 0x55;
constexpr uint8_t BPB_SIGNATURE_SECOND = 0xAA;
constexpr size_t BPB_SIGNATURE_SECOND_STEP = 1;
constexpr uint32_t MAX_CLUSTER_SECTORS = 128;
constexpr uint32_t MAX_FAT_COUNT = 4;
constexpr uint32_t MIN_FAT32_CLUSTERS = 65525;
constexpr uint32_t MAX_FAT32_CLUSTERS = 0x0FFFFFF5;
constexpr uint32_t FIRST_DATA_CLUSTER = 2;

constexpr uint32_t FAT_VALUE_MASK = 0x0FFFFFFF;
constexpr uint32_t FAT_END_OF_CHAIN_MIN = 0x0FFFFFF8;
constexpr uint32_t FAT_BAD_CLUSTER = 0x0FFFFFF7;
constexpr uint32_t FAT_END_OF_CHAIN = 0x0FFFFFFF;
constexpr uint32_t FAT_FREE = 0;
constexpr uint32_t FAT_STATE_ENTRY = 1;
constexpr uint32_t FAT_CLEAN_SHUTDOWN_BIT = 0x08000000;
constexpr uint32_t FAT_RESERVED_MASK = 0xF0000000;

constexpr uint32_t FSINFO_LEAD_SIGNATURE = 0x41615252;
constexpr uint32_t FSINFO_STRUCT_SIGNATURE = 0x61417272;
constexpr uint32_t FSINFO_TRAIL_SIGNATURE = 0xAA550000;
constexpr size_t FSINFO_LEAD_OFFSET = 0;
constexpr size_t FSINFO_STRUCT_OFFSET = 484;
constexpr size_t FSINFO_FREE_OFFSET = 488;
constexpr size_t FSINFO_NEXT_OFFSET = 492;
constexpr size_t FSINFO_TRAIL_OFFSET = 508;
constexpr uint32_t FSINFO_UNKNOWN = 0xFFFFFFFF;

constexpr uint8_t DIRECTORY_END = 0x00;
constexpr uint8_t DIRECTORY_DELETED = 0xE5;
constexpr uint8_t DIRECTORY_KANJI_ESCAPE = 0x05;
constexpr size_t ENTRY_NAME_OFFSET = 0;
constexpr size_t ENTRY_BASE_BYTES = 8;
constexpr size_t ENTRY_EXTENSION_BYTES = 3;
constexpr size_t ENTRY_SHORT_NAME_BYTES = ENTRY_BASE_BYTES + ENTRY_EXTENSION_BYTES;
constexpr size_t ENTRY_ATTRIBUTES_OFFSET = 11;
constexpr size_t ENTRY_CASE_OFFSET = 12;
constexpr size_t ENTRY_CREATE_TIME_OFFSET = 14;
constexpr size_t ENTRY_CREATE_DATE_OFFSET = 16;
constexpr size_t ENTRY_ACCESS_DATE_OFFSET = 18;
constexpr size_t ENTRY_CLUSTER_HIGH_OFFSET = 20;
constexpr size_t ENTRY_WRITE_TIME_OFFSET = 22;
constexpr size_t ENTRY_WRITE_DATE_OFFSET = 24;
constexpr size_t ENTRY_CLUSTER_LOW_OFFSET = 26;
constexpr size_t ENTRY_SIZE_OFFSET = 28;
constexpr uint16_t FAT_EPOCH_DATE = 0x0021;
constexpr uint16_t FAT_EPOCH_TIME = 0;
constexpr uint8_t ATTRIBUTE_ARCHIVE = 0x20;
constexpr char SHORT_NAME_SYMBOLS[] = "$%'-_@~`!(){}^#&";
constexpr size_t MAX_SHORT_NAME_BYTES = ENTRY_BASE_BYTES + 1 + ENTRY_EXTENSION_BYTES;
constexpr char DIGIT_FIRST = '0';
constexpr char DIGIT_LAST = '9';
constexpr uint8_t CASE_LOWER_BASE = 0x08;
constexpr uint8_t CASE_LOWER_EXTENSION = 0x10;
constexpr uint8_t ATTRIBUTE_VOLUME_LABEL = 0x08;
constexpr uint8_t ATTRIBUTE_DIRECTORY = 0x10;
constexpr uint8_t ATTRIBUTE_LONG_NAME = 0x0F;
constexpr uint8_t ATTRIBUTE_LONG_NAME_MASK = 0x3F;
constexpr char SPACE = ' ';
constexpr char DOT = '.';
constexpr char UNKNOWN_CHARACTER = '?';

constexpr uint8_t LONG_LAST_FLAG = 0x40;
constexpr uint8_t LONG_INDEX_MASK = 0x1F;
constexpr size_t LONG_CHECKSUM_OFFSET = 13;
constexpr size_t LONG_CHARS_PER_ENTRY = 13;
constexpr size_t LONG_MAX_ENTRIES = 20;
constexpr size_t LONG_CHAR_BYTES = 2;
constexpr size_t LONG_FIRST_GROUP_OFFSET = 1;
constexpr size_t LONG_FIRST_GROUP_CHARS = 5;
constexpr size_t LONG_SECOND_GROUP_OFFSET = 14;
constexpr size_t LONG_SECOND_GROUP_CHARS = 6;
constexpr size_t LONG_THIRD_GROUP_OFFSET = 28;
constexpr size_t LONG_THIRD_GROUP_CHARS = 2;
constexpr uint32_t UTF8_ONE_BYTE_LIMIT = 0x80;
constexpr uint32_t UTF8_TWO_BYTE_LIMIT = 0x800;
constexpr uint32_t UTF8_THREE_BYTE_LIMIT = 0x10000;
constexpr uint32_t UTF8_TWO_BYTE_MARK = 0xC0;
constexpr uint32_t UTF8_THREE_BYTE_MARK = 0xE0;
constexpr uint32_t UTF8_FOUR_BYTE_MARK = 0xF0;
constexpr uint32_t UTF8_CONTINUATION_MARK = 0x80;
constexpr uint32_t UTF8_PAYLOAD_MASK = 0x3F;
constexpr unsigned UTF8_PAYLOAD_BITS = 6;
constexpr uint32_t HIGH_SURROGATE_FIRST = 0xD800;
constexpr uint32_t HIGH_SURROGATE_LAST = 0xDBFF;
constexpr uint32_t LOW_SURROGATE_FIRST = 0xDC00;
constexpr uint32_t LOW_SURROGATE_LAST = 0xDFFF;
constexpr uint32_t SUPPLEMENTARY_BASE = 0x10000;
constexpr unsigned SURROGATE_BITS = 10;
constexpr uint16_t LONG_NAME_END = 0x0000;
constexpr size_t MAX_NAME_CHARS = LONG_MAX_ENTRIES * LONG_CHARS_PER_ENTRY;
constexpr size_t UTF8_MAX_UNIT_BYTES = 3;
constexpr size_t MAX_NAME_BYTES = MAX_NAME_CHARS * UTF8_MAX_UNIT_BYTES + 1;
constexpr unsigned CHECKSUM_RIGHT_SHIFT = 1;
constexpr unsigned CHECKSUM_LEFT_SHIFT = 7;
constexpr uint8_t CHECKSUM_BYTE_MASK = 0xFF;
constexpr char LETTER_UPPER_A = 'A';
constexpr char LETTER_UPPER_Z = 'Z';
constexpr char LETTER_LOWER_A = 'a';
constexpr char LETTER_LOWER_Z = 'z';
constexpr uint32_t LOW_HALF_MASK = 0xFFFF;
constexpr char CASE_DISTANCE = LETTER_LOWER_A - LETTER_UPPER_A;
constexpr size_t LONG_FIRST_INDEX = 1;
constexpr size_t DOT_SECOND_BYTE = 1;
constexpr size_t DOT_PADDING_START = 2;
constexpr uint16_t LONG_PADDING_UNIT = 0xFFFF;
constexpr char FORBIDDEN_LONG_CHARACTERS[] = "\"*/:<>?\\|";
constexpr char DELETE_CHARACTER = 0x7F;
constexpr char FIRST_PRINTABLE = 0x20;
constexpr char TILDE = '~';
constexpr char UNDERSCORE = '_';
constexpr uint32_t UTF8_CONTINUATION_MASK = 0xC0;
constexpr uint32_t UTF8_TWO_LEAD_MASK = 0xE0;
constexpr uint32_t UTF8_THREE_LEAD_MASK = 0xF0;
constexpr uint32_t UTF8_FOUR_LEAD_MASK = 0xF8;
constexpr uint32_t UTF8_TWO_PAYLOAD_MASK = 0x1F;
constexpr uint32_t UTF8_THREE_PAYLOAD_MASK = 0x0F;
constexpr uint32_t UTF8_FOUR_PAYLOAD_MASK = 0x07;
constexpr uint32_t MAX_CODE_POINT = 0x10FFFF;
constexpr uint32_t SURROGATE_MASK = 0x3FF;
constexpr uint32_t DECIMAL_RADIX = 10;
constexpr uint32_t MAX_ALIAS_TAIL_DIGITS = 6;
constexpr uint32_t MAX_ALIAS_TAIL = 999999;

constexpr unsigned SHIFT_8 = 8;
constexpr unsigned SHIFT_16 = 16;
constexpr unsigned SHIFT_24 = 24;
constexpr size_t THIRD_BYTE = 2;
constexpr size_t FOURTH_BYTE = 3;

enum class BpbError : uint32_t {
	NONE = 0,
	BAD_SIGNATURE,
	BAD_SECTOR_SIZE,
	BAD_CLUSTER_SIZE,
	BAD_LAYOUT,
	NOT_FAT32,
};

struct Bpb {
	uint32_t sectorsPerCluster;
	uint32_t reservedSectors;
	uint32_t fatCount;
	uint32_t fatSectors;
	uint32_t fsInfoSector;
	uint32_t rootCluster;
	uint32_t clusterCount;
	uint32_t dataStart;
	uint32_t totalSectors;

	uint32_t clusterBytes() const { return sectorsPerCluster * uint32_t(SECTOR_BYTES); }

	bool isDataCluster(uint32_t cluster) const {
		return cluster >= FIRST_DATA_CLUSTER && cluster - FIRST_DATA_CLUSTER < clusterCount;
	}

	uint32_t clusterSector(uint32_t cluster) const {
		return dataStart + (cluster - FIRST_DATA_CLUSTER) * sectorsPerCluster;
	}
};

struct DirEntry {
	char name[MAX_NAME_BYTES];
	uint8_t attributes;
	uint32_t cluster;
	uint32_t size;

	bool isDirectory() const { return (attributes & ATTRIBUTE_DIRECTORY) != 0; }
};

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

inline char lower(char character) {
	return character >= LETTER_UPPER_A && character <= LETTER_UPPER_Z ? char(character + CASE_DISTANCE) : character;
}

} // namespace detail

inline BpbError parseBpb(const uint8_t *sector, uint64_t partitionSectors, Bpb &bpb) {
	if (sector[BPB_SIGNATURE_OFFSET] != BPB_SIGNATURE_FIRST ||
		sector[BPB_SIGNATURE_OFFSET + BPB_SIGNATURE_SECOND_STEP] != BPB_SIGNATURE_SECOND)
		return BpbError::BAD_SIGNATURE;
	if (detail::readLe16(sector + BPB_SECTOR_BYTES_OFFSET) != SECTOR_BYTES)
		return BpbError::BAD_SECTOR_SIZE;

	uint32_t clusterSectors = sector[BPB_CLUSTER_SECTORS_OFFSET];
	if (!detail::isPowerOfTwo(clusterSectors) || clusterSectors > MAX_CLUSTER_SECTORS)
		return BpbError::BAD_CLUSTER_SIZE;

	uint32_t reserved = detail::readLe16(sector + BPB_RESERVED_OFFSET);
	uint32_t fatCount = sector[BPB_FAT_COUNT_OFFSET];
	uint32_t fatSize = detail::readLe32(sector + BPB_FAT_SIZE32_OFFSET);
	uint32_t total = detail::readLe16(sector + BPB_TOTAL16_OFFSET);
	if (total == 0) total = detail::readLe32(sector + BPB_TOTAL32_OFFSET);

	if (detail::readLe16(sector + BPB_ROOT_ENTRIES_OFFSET) != 0 ||
		detail::readLe16(sector + BPB_FAT_SIZE16_OFFSET) != 0 || fatSize == 0)
		return BpbError::NOT_FAT32;
	if (detail::readLe16(sector + BPB_VERSION_OFFSET) != 0) return BpbError::BAD_LAYOUT;
	if (reserved == 0 || fatCount == 0 || fatCount > MAX_FAT_COUNT || total == 0 ||
		total > partitionSectors)
		return BpbError::BAD_LAYOUT;

	uint64_t overhead = uint64_t(reserved) + uint64_t(fatCount) * fatSize;
	if (overhead >= total) return BpbError::BAD_LAYOUT;

	uint32_t dataSectors = total - static_cast<uint32_t>(overhead);
	uint32_t clusters = dataSectors / clusterSectors;
	if (clusters < MIN_FAT32_CLUSTERS) return BpbError::NOT_FAT32;
	if (clusters >= MAX_FAT32_CLUSTERS) return BpbError::BAD_LAYOUT;
	if (uint64_t(fatSize) * FAT_ENTRIES_PER_SECTOR < uint64_t(clusters) + FIRST_DATA_CLUSTER)
		return BpbError::BAD_LAYOUT;

	uint32_t root = detail::readLe32(sector + BPB_ROOT_CLUSTER_OFFSET);
	if (root < FIRST_DATA_CLUSTER || root - FIRST_DATA_CLUSTER >= clusters)
		return BpbError::BAD_LAYOUT;

	bpb.sectorsPerCluster = clusterSectors;
	bpb.reservedSectors = reserved;
	bpb.fatCount = fatCount;
	bpb.fatSectors = fatSize;
	bpb.fsInfoSector = detail::readLe16(sector + BPB_FSINFO_OFFSET);
	bpb.rootCluster = root;
	bpb.clusterCount = clusters;
	bpb.dataStart = static_cast<uint32_t>(overhead);
	bpb.totalSectors = total;
	return BpbError::NONE;
}

inline uint32_t fatValue(const uint8_t *fatSector, uint32_t indexInSector) {
	return detail::readLe32(fatSector + size_t(indexInSector) * FAT_ENTRY_BYTES) &
		   FAT_VALUE_MASK;
}

inline bool isEndOfChain(uint32_t value) { return value >= FAT_END_OF_CHAIN_MIN; }

inline void setFatValue(uint8_t *fatSector, uint32_t indexInSector, uint32_t value) {
	uint8_t *entry = fatSector + size_t(indexInSector) * FAT_ENTRY_BYTES;
	uint32_t kept = detail::readLe32(entry) & FAT_RESERVED_MASK;
	detail::writeLe32(entry, kept | (value & FAT_VALUE_MASK));
}

inline bool validFsInfo(const uint8_t *sector) {
	return detail::readLe32(sector + FSINFO_LEAD_OFFSET) == FSINFO_LEAD_SIGNATURE &&
		   detail::readLe32(sector + FSINFO_STRUCT_OFFSET) == FSINFO_STRUCT_SIGNATURE &&
		   detail::readLe32(sector + FSINFO_TRAIL_OFFSET) == FSINFO_TRAIL_SIGNATURE;
}

inline uint32_t fsInfoFree(const uint8_t *sector) {
	return detail::readLe32(sector + FSINFO_FREE_OFFSET);
}

inline uint32_t fsInfoNext(const uint8_t *sector) {
	return detail::readLe32(sector + FSINFO_NEXT_OFFSET);
}

inline void storeFsInfo(uint8_t *sector, uint32_t freeClusters, uint32_t nextFree) {
	detail::writeLe32(sector + FSINFO_FREE_OFFSET, freeClusters);
	detail::writeLe32(sector + FSINFO_NEXT_OFFSET, nextFree);
}

inline bool isShortNameCharacter(char character) {
	if (character >= DIGIT_FIRST && character <= DIGIT_LAST) return true;
	if (character >= LETTER_UPPER_A && character <= LETTER_UPPER_Z) return true;
	for (size_t index = 0; SHORT_NAME_SYMBOLS[index] != '\0'; index++)
		if (character == SHORT_NAME_SYMBOLS[index]) return true;
	return false;
}

inline bool encodeShortNamePart(const char *part, size_t length, uint8_t *target,
								bool &lowered) {
	bool sawUpper = false;
	bool sawLower = false;
	for (size_t index = 0; index < length; index++) {
		char character = part[index];
		if (character >= LETTER_LOWER_A && character <= LETTER_LOWER_Z) {
			sawLower = true;
			character = char(character - CASE_DISTANCE);
		} else if (character >= LETTER_UPPER_A && character <= LETTER_UPPER_Z) {
			sawUpper = true;
		}
		if (!isShortNameCharacter(character)) return false;
		target[index] = uint8_t(character);
	}
	if (sawUpper && sawLower) return false;
	lowered = sawLower;
	return true;
}

inline bool encodeShortName(const char *name, size_t length, uint8_t *shortName,
							uint8_t &caseFlags) {
	if (length == 0 || length > MAX_SHORT_NAME_BYTES) return false;

	size_t dot = length;
	for (size_t index = 0; index < length; index++)
		if (name[index] == DOT) dot = index;

	size_t baseLength = dot;
	size_t extensionLength = dot == length ? 0 : length - dot - 1;
	if (baseLength == 0 || baseLength > ENTRY_BASE_BYTES ||
		extensionLength > ENTRY_EXTENSION_BYTES || (dot != length && extensionLength == 0))
		return false;

	for (size_t index = 0; index < ENTRY_SHORT_NAME_BYTES; index++)
		shortName[index] = uint8_t(SPACE);

	bool baseLowered = false;
	bool extensionLowered = false;
	if (!encodeShortNamePart(name, baseLength, shortName, baseLowered) ||
		!encodeShortNamePart(name + dot + 1, extensionLength, shortName + ENTRY_BASE_BYTES,
							 extensionLowered))
		return false;

	caseFlags = uint8_t((baseLowered ? CASE_LOWER_BASE : 0) |
						(extensionLowered ? CASE_LOWER_EXTENSION : 0));
	return true;
}

inline void patchEntryLocation(uint8_t *raw, uint32_t cluster, uint32_t size) {
	detail::writeLe16(raw + ENTRY_CLUSTER_HIGH_OFFSET, cluster >> SHIFT_16);
	detail::writeLe16(raw + ENTRY_CLUSTER_LOW_OFFSET, cluster & LOW_HALF_MASK);
	detail::writeLe32(raw + ENTRY_SIZE_OFFSET, size);
}

inline void touchEntry(uint8_t *raw, uint16_t date, uint16_t time) {
	detail::writeLe16(raw + ENTRY_ACCESS_DATE_OFFSET, date);
	detail::writeLe16(raw + ENTRY_WRITE_TIME_OFFSET, time);
	detail::writeLe16(raw + ENTRY_WRITE_DATE_OFFSET, date);
}

inline void encodeEntry(uint8_t *raw, const uint8_t *shortName, uint8_t caseFlags,
						uint8_t attributes, uint32_t cluster, uint32_t size,
						uint16_t date = FAT_EPOCH_DATE, uint16_t time = FAT_EPOCH_TIME) {
	for (size_t index = 0; index < ENTRY_BYTES; index++)
		raw[index] = 0;
	for (size_t index = 0; index < ENTRY_SHORT_NAME_BYTES; index++)
		raw[ENTRY_NAME_OFFSET + index] = shortName[index];

	raw[ENTRY_ATTRIBUTES_OFFSET] = attributes;
	raw[ENTRY_CASE_OFFSET] = caseFlags;
	detail::writeLe16(raw + ENTRY_CREATE_TIME_OFFSET, time);
	detail::writeLe16(raw + ENTRY_CREATE_DATE_OFFSET, date);
	touchEntry(raw, date, time);
	patchEntryLocation(raw, cluster, size);
}

inline bool namesEqual(const char *first, const char *second) {
	size_t index = 0;
	for (; first[index] != '\0' && second[index] != '\0'; index++)
		if (detail::lower(first[index]) != detail::lower(second[index])) return false;
	return first[index] == second[index];
}

inline uint8_t shortNameChecksum(const uint8_t *shortName) {
	uint8_t sum = 0;
	for (size_t index = 0; index < ENTRY_SHORT_NAME_BYTES; index++)
		sum = uint8_t(((sum >> CHECKSUM_RIGHT_SHIFT) | (sum << CHECKSUM_LEFT_SHIFT)) &
					  CHECKSUM_BYTE_MASK) +
			  shortName[index];
	return sum;
}

enum class NamePlan : uint32_t {
	SHORT = 0,
	CASED = 1,
	LOSSY = 2,
};

inline NamePlan planName(const char *name, size_t length, uint8_t *shortName,
						 uint8_t &caseFlags) {
	if (encodeShortName(name, length, shortName, caseFlags)) return NamePlan::SHORT;
	if (length > MAX_SHORT_NAME_BYTES) return NamePlan::LOSSY;

	char upper[MAX_SHORT_NAME_BYTES];
	for (size_t index = 0; index < length; index++) {
		char character = name[index];
		upper[index] = character >= LETTER_LOWER_A && character <= LETTER_LOWER_Z
						   ? char(character - CASE_DISTANCE)
						   : character;
	}
	uint8_t ignored = 0;
	if (!encodeShortName(upper, length, shortName, ignored)) return NamePlan::LOSSY;
	caseFlags = 0;
	return NamePlan::CASED;
}

inline void makeLossyAlias(const char *name, size_t length, uint32_t tail,
						   uint8_t *shortName) {
	for (size_t index = 0; index < ENTRY_SHORT_NAME_BYTES; index++)
		shortName[index] = uint8_t(SPACE);

	size_t start = 0;
	while (start < length && name[start] == DOT)
		start++;
	size_t dot = length;
	for (size_t index = start; index < length; index++)
		if (name[index] == DOT) dot = index;

	char digits[MAX_ALIAS_TAIL_DIGITS + 1];
	size_t digitCount = 0;
	for (uint32_t rest = tail; rest != 0; rest /= DECIMAL_RADIX)
		digits[digitCount++] = char(DIGIT_FIRST + rest % DECIMAL_RADIX);
	size_t keep = ENTRY_BASE_BYTES - 1 - digitCount;

	size_t used = 0;
	for (size_t index = start; index < dot && used < keep; index++) {
		uint8_t byte = uint8_t(name[index]);
		if (name[index] == SPACE || (byte & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_MARK)
			continue;
		char character = name[index] >= LETTER_LOWER_A && name[index] <= LETTER_LOWER_Z
							 ? char(name[index] - CASE_DISTANCE)
							 : name[index];
		shortName[used++] = uint8_t(isShortNameCharacter(character) ? character : UNDERSCORE);
	}
	if (used == 0) shortName[used++] = uint8_t(UNDERSCORE);
	shortName[used++] = uint8_t(TILDE);
	for (size_t index = digitCount; index > 0; index--)
		shortName[used++] = uint8_t(digits[index - 1]);

	size_t extensionUsed = 0;
	for (size_t index = dot + 1; index < length && extensionUsed < ENTRY_EXTENSION_BYTES;
		 index++) {
		uint8_t byte = uint8_t(name[index]);
		if (name[index] == SPACE || (byte & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_MARK)
			continue;
		char character = name[index] >= LETTER_LOWER_A && name[index] <= LETTER_LOWER_Z
							 ? char(name[index] - CASE_DISTANCE)
							 : name[index];
		shortName[ENTRY_BASE_BYTES + extensionUsed++] =
			uint8_t(isShortNameCharacter(character) ? character : UNDERSCORE);
	}
}

inline bool isForbiddenInLongName(uint32_t code) {
	if (code < uint32_t(FIRST_PRINTABLE) || code == uint32_t(DELETE_CHARACTER)) return true;
	for (size_t index = 0; FORBIDDEN_LONG_CHARACTERS[index] != '\0'; index++)
		if (code == uint32_t(FORBIDDEN_LONG_CHARACTERS[index])) return true;
	return false;
}

inline bool decodeUtf8Name(const char *text, size_t length, uint16_t *units, size_t capacity,
						   size_t &count) {
	count = 0;
	size_t index = 0;
	while (index < length) {
		uint32_t lead = uint8_t(text[index]);
		uint32_t code = 0;
		size_t extra = 0;
		uint32_t minimum = 0;
		if (lead < UTF8_ONE_BYTE_LIMIT) {
			code = lead;
		} else if ((lead & UTF8_TWO_LEAD_MASK) == UTF8_TWO_BYTE_MARK) {
			code = lead & UTF8_TWO_PAYLOAD_MASK;
			extra = 1;
			minimum = UTF8_ONE_BYTE_LIMIT;
		} else if ((lead & UTF8_THREE_LEAD_MASK) == UTF8_THREE_BYTE_MARK) {
			code = lead & UTF8_THREE_PAYLOAD_MASK;
			extra = 2;
			minimum = UTF8_TWO_BYTE_LIMIT;
		} else if ((lead & UTF8_FOUR_LEAD_MASK) == UTF8_FOUR_BYTE_MARK) {
			code = lead & UTF8_FOUR_PAYLOAD_MASK;
			extra = 3;
			minimum = UTF8_THREE_BYTE_LIMIT;
		} else {
			return false;
		}
		if (index + extra >= length) return false;
		for (size_t step = 1; step <= extra; step++) {
			uint32_t next = uint8_t(text[index + step]);
			if ((next & UTF8_CONTINUATION_MASK) != UTF8_CONTINUATION_MARK) return false;
			code = (code << UTF8_PAYLOAD_BITS) | (next & UTF8_PAYLOAD_MASK);
		}
		index += extra + 1;

		if (code < minimum || code > MAX_CODE_POINT ||
			(code >= HIGH_SURROGATE_FIRST && code <= LOW_SURROGATE_LAST) ||
			isForbiddenInLongName(code))
			return false;

		if (code >= SUPPLEMENTARY_BASE) {
			if (count + 2 > capacity) return false;
			uint32_t offset = code - SUPPLEMENTARY_BASE;
			units[count++] = uint16_t(HIGH_SURROGATE_FIRST + (offset >> SURROGATE_BITS));
			units[count++] = uint16_t(LOW_SURROGATE_FIRST + (offset & SURROGATE_MASK));
		} else {
			if (count + 1 > capacity) return false;
			units[count++] = uint16_t(code);
		}
	}

	if (count == 0) return false;
	uint16_t last = units[count - 1];
	return last != uint16_t(DOT) && last != uint16_t(SPACE);
}

inline size_t longSlotOffset(size_t slot) {
	if (slot < LONG_FIRST_GROUP_CHARS) return LONG_FIRST_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
	slot -= LONG_FIRST_GROUP_CHARS;
	if (slot < LONG_SECOND_GROUP_CHARS)
		return LONG_SECOND_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
	slot -= LONG_SECOND_GROUP_CHARS;
	return LONG_THIRD_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
}

inline size_t longEntryCount(size_t unitCount) {
	return (unitCount + LONG_CHARS_PER_ENTRY - 1) / LONG_CHARS_PER_ENTRY;
}

inline void encodeLongEntry(uint8_t *raw, size_t sequence, bool last, uint8_t checksum,
							const uint16_t *units, size_t unitCount) {
	for (size_t index = 0; index < ENTRY_BYTES; index++)
		raw[index] = 0;
	raw[ENTRY_NAME_OFFSET] = uint8_t(sequence | (last ? LONG_LAST_FLAG : 0));
	raw[ENTRY_ATTRIBUTES_OFFSET] = ATTRIBUTE_LONG_NAME;
	raw[LONG_CHECKSUM_OFFSET] = checksum;

	for (size_t slot = 0; slot < LONG_CHARS_PER_ENTRY; slot++) {
		size_t position = (sequence - 1) * LONG_CHARS_PER_ENTRY + slot;
		uint32_t value = position < unitCount    ? units[position]
						 : position == unitCount ? uint32_t(LONG_NAME_END)
												 : uint32_t(LONG_PADDING_UNIT);
		detail::writeLe16(raw + longSlotOffset(slot), value);
	}
}

inline void dotShortName(bool parent, uint8_t *shortName) {
	for (size_t index = 0; index < ENTRY_SHORT_NAME_BYTES; index++)
		shortName[index] = uint8_t(SPACE);
	shortName[ENTRY_NAME_OFFSET] = uint8_t(DOT);
	if (parent) shortName[DOT_SECOND_BYTE] = uint8_t(DOT);
}

class DirectoryParser {
  public:
	enum class Result : uint32_t {
		SKIP = 0,
		ENTRY = 1,
		END = 2,
	};

	void reset() { clearLongName(); }

	Result feed(const uint8_t *raw, DirEntry &entry) {
		if (raw[ENTRY_NAME_OFFSET] == DIRECTORY_END) return Result::END;
		if (raw[ENTRY_NAME_OFFSET] == DIRECTORY_DELETED) {
			clearLongName();
			return Result::SKIP;
		}

		uint8_t attributes = raw[ENTRY_ATTRIBUTES_OFFSET];
		if ((attributes & ATTRIBUTE_LONG_NAME_MASK) == ATTRIBUTE_LONG_NAME) {
			addLongPart(raw);
			return Result::SKIP;
		}

		bool labeled = (attributes & ATTRIBUTE_VOLUME_LABEL) != 0;
		bool useLong = nextIndex == LONG_FIRST_INDEX && checksum == shortNameChecksum(raw);
		bool dotEntry = raw[ENTRY_NAME_OFFSET] == uint8_t(DOT);
		if (labeled || (dotEntry && !useLong && isDotName(raw))) {
			clearLongName();
			return Result::SKIP;
		}

		if (useLong)
			copyLongName(entry);
		else
			buildShortName(raw, entry);
		clearLongName();

		entry.attributes = attributes;
		entry.cluster = (uint32_t(detail::readLe16(raw + ENTRY_CLUSTER_HIGH_OFFSET))
						 << SHIFT_16) |
						detail::readLe16(raw + ENTRY_CLUSTER_LOW_OFFSET);
		entry.size = detail::readLe32(raw + ENTRY_SIZE_OFFSET);
		return Result::ENTRY;
	}

  private:
	uint16_t longName[MAX_NAME_CHARS + 1];
	size_t nextIndex = 0;
	size_t longLength = 0;
	uint8_t checksum = 0;

	void clearLongName() {
		nextIndex = 0;
		longLength = 0;
	}

	static bool isDotName(const uint8_t *raw) {
		if (raw[DOT_SECOND_BYTE] == uint8_t(SPACE)) return true;
		if (raw[DOT_SECOND_BYTE] != uint8_t(DOT)) return false;
		for (size_t index = DOT_PADDING_START; index < ENTRY_SHORT_NAME_BYTES; index++)
			if (raw[index] != uint8_t(SPACE)) return false;
		return true;
	}

	void addLongPart(const uint8_t *raw) {
		uint8_t order = raw[ENTRY_NAME_OFFSET];
		size_t index = order & LONG_INDEX_MASK;
		if ((order & LONG_LAST_FLAG) != 0) {
			if (index == 0 || index > LONG_MAX_ENTRIES) return clearLongName();
			clearLongName();
			checksum = raw[LONG_CHECKSUM_OFFSET];
			longLength = index * LONG_CHARS_PER_ENTRY;
		} else if (nextIndex == 0 || index != nextIndex - 1 ||
				   raw[LONG_CHECKSUM_OFFSET] != checksum) {
			return clearLongName();
		}

		nextIndex = index;
		uint16_t *target = longName + (index - 1) * LONG_CHARS_PER_ENTRY;
		copyGroup(raw + LONG_FIRST_GROUP_OFFSET, LONG_FIRST_GROUP_CHARS, target);
		copyGroup(raw + LONG_SECOND_GROUP_OFFSET, LONG_SECOND_GROUP_CHARS,
				  target + LONG_FIRST_GROUP_CHARS);
		copyGroup(raw + LONG_THIRD_GROUP_OFFSET, LONG_THIRD_GROUP_CHARS,
				  target + LONG_FIRST_GROUP_CHARS + LONG_SECOND_GROUP_CHARS);
	}

	static void copyGroup(const uint8_t *source, size_t chars, uint16_t *target) {
		for (size_t index = 0; index < chars; index++)
			target[index] = detail::readLe16(source + index * LONG_CHAR_BYTES);
	}

	static size_t encode(uint32_t code, char *target) {
		if (code < UTF8_ONE_BYTE_LIMIT) {
			target[0] = char(code);
			return 1;
		}
		if (code < UTF8_TWO_BYTE_LIMIT) {
			target[0] = char(UTF8_TWO_BYTE_MARK | (code >> UTF8_PAYLOAD_BITS));
			target[1] = char(UTF8_CONTINUATION_MARK | (code & UTF8_PAYLOAD_MASK));
			return 2;
		}
		if (code < UTF8_THREE_BYTE_LIMIT) {
			target[0] = char(UTF8_THREE_BYTE_MARK | (code >> (2 * UTF8_PAYLOAD_BITS)));
			target[1] = char(UTF8_CONTINUATION_MARK |
							 ((code >> UTF8_PAYLOAD_BITS) & UTF8_PAYLOAD_MASK));
			target[2] = char(UTF8_CONTINUATION_MARK | (code & UTF8_PAYLOAD_MASK));
			return 3;
		}
		target[0] = char(UTF8_FOUR_BYTE_MARK | (code >> (3 * UTF8_PAYLOAD_BITS)));
		target[1] = char(UTF8_CONTINUATION_MARK |
						 ((code >> (2 * UTF8_PAYLOAD_BITS)) & UTF8_PAYLOAD_MASK));
		target[2] = char(UTF8_CONTINUATION_MARK |
						 ((code >> UTF8_PAYLOAD_BITS) & UTF8_PAYLOAD_MASK));
		target[3] = char(UTF8_CONTINUATION_MARK | (code & UTF8_PAYLOAD_MASK));
		return 4;
	}

	void copyLongName(DirEntry &entry) const {
		size_t length = 0;
		size_t index = 0;
		while (index < longLength && longName[index] != LONG_NAME_END) {
			uint32_t unit = longName[index++];
			uint32_t code = unit;
			if (unit >= HIGH_SURROGATE_FIRST && unit <= HIGH_SURROGATE_LAST) {
				uint32_t next = index < longLength ? longName[index] : LONG_NAME_END;
				if (next >= LOW_SURROGATE_FIRST && next <= LOW_SURROGATE_LAST) {
					code = SUPPLEMENTARY_BASE +
						   (((unit - HIGH_SURROGATE_FIRST) << SURROGATE_BITS) |
							(next - LOW_SURROGATE_FIRST));
					index++;
				} else {
					code = uint32_t(UNKNOWN_CHARACTER);
				}
			} else if (unit >= LOW_SURROGATE_FIRST && unit <= LOW_SURROGATE_LAST) {
				code = uint32_t(UNKNOWN_CHARACTER);
			}
			length += encode(code, entry.name + length);
		}
		entry.name[length] = '\0';
	}

	static void buildShortName(const uint8_t *raw, DirEntry &entry) {
		uint8_t flags = raw[ENTRY_CASE_OFFSET];
		size_t length = 0;
		size_t baseLength = ENTRY_BASE_BYTES;
		while (baseLength > 0 && raw[baseLength - 1] == uint8_t(SPACE))
			baseLength--;
		for (size_t index = 0; index < baseLength; index++) {
			char character = char(raw[index]);
			if (index == ENTRY_NAME_OFFSET && raw[ENTRY_NAME_OFFSET] == DIRECTORY_KANJI_ESCAPE)
				character = char(DIRECTORY_DELETED);
			entry.name[length++] = (flags & CASE_LOWER_BASE) != 0
									   ? detail::lower(character)
									   : character;
		}

		size_t extensionLength = ENTRY_EXTENSION_BYTES;
		while (extensionLength > 0 &&
			   raw[ENTRY_BASE_BYTES + extensionLength - 1] == uint8_t(SPACE))
			extensionLength--;
		if (extensionLength > 0) entry.name[length++] = DOT;
		for (size_t index = 0; index < extensionLength; index++) {
			char character = char(raw[ENTRY_BASE_BYTES + index]);
			entry.name[length++] = (flags & CASE_LOWER_EXTENSION) != 0
									   ? detail::lower(character)
									   : character;
		}
		entry.name[length] = '\0';
	}
};

} // namespace cock::abi::fat32

#endif // !SHARED_COCK_ABI_FAT32_HPP
