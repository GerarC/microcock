/**
 * @file fat32.hpp
 * @brief On-disk layout and pure helpers of the FAT32 file system.
 *
 * Everything here is parsing and encoding over byte buffers: no I/O and no dependencies, so the `fat` server and the host tests
 * share it. All on-disk integers are little endian. Names ending in `_OFFSET` are byte offsets inside the structure
 * named by their prefix: `BPB_` the boot sector, `FSINFO_` the file system information sector, `ENTRY_` a 32 byte
 * directory entry and `LONG_` a long file name entry. Only FAT32 with 512 byte sectors is accepted (FAT12 and FAT16
 * are detected by their cluster count and refused). Names are UTF-8 on the wire and UTF-16 in long name entries.
 */
#ifndef SHARED_COCK_ABI_FAT32_HPP
#define SHARED_COCK_ABI_FAT32_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::fat32 {

/** Size of a sector in bytes; the only size accepted. */
constexpr size_t SECTOR_BYTES = 512;
/** Size of a directory entry. */
constexpr size_t ENTRY_BYTES = 32;
/** Directory entries in one sector (16). */
constexpr size_t ENTRIES_PER_SECTOR = SECTOR_BYTES / ENTRY_BYTES;
/** Size of one FAT table entry. */
constexpr size_t FAT_ENTRY_BYTES = 4;
/** FAT entries in one sector (128). */
constexpr size_t FAT_ENTRIES_PER_SECTOR = SECTOR_BYTES / FAT_ENTRY_BYTES;

/** Boot sector field: bytes per sector (16 bit). */
constexpr size_t BPB_SECTOR_BYTES_OFFSET = 11;
/** Boot sector field: sectors per cluster (8 bit). */
constexpr size_t BPB_CLUSTER_SECTORS_OFFSET = 13;
/** Boot sector field: reserved sectors before the first FAT (16 bit). */
constexpr size_t BPB_RESERVED_OFFSET = 14;
/** Boot sector field: number of FAT copies (8 bit). */
constexpr size_t BPB_FAT_COUNT_OFFSET = 16;
/** Boot sector field: root directory entries; must be 0 on FAT32 (16 bit). */
constexpr size_t BPB_ROOT_ENTRIES_OFFSET = 17;
/** Boot sector field: 16 bit total sector count; 0 when the 32 bit field is used. */
constexpr size_t BPB_TOTAL16_OFFSET = 19;
/** Boot sector field: 16 bit FAT size; must be 0 on FAT32. */
constexpr size_t BPB_FAT_SIZE16_OFFSET = 22;
/** Boot sector field: 32 bit total sector count. */
constexpr size_t BPB_TOTAL32_OFFSET = 32;
/** Boot sector field: sectors per FAT (32 bit). */
constexpr size_t BPB_FAT_SIZE32_OFFSET = 36;
/** Boot sector field: file system version; must be 0 (16 bit). */
constexpr size_t BPB_VERSION_OFFSET = 42;
/** Boot sector field: first cluster of the root directory (32 bit). */
constexpr size_t BPB_ROOT_CLUSTER_OFFSET = 44;
/** Boot sector field: sector of the FSInfo structure (16 bit). */
constexpr size_t BPB_FSINFO_OFFSET = 48;
/** Offset of the first boot signature byte. */
constexpr size_t BPB_SIGNATURE_OFFSET = 510;
/** Value of the first boot signature byte. */
constexpr uint8_t BPB_SIGNATURE_FIRST = 0x55;
/** Value of the second boot signature byte. */
constexpr uint8_t BPB_SIGNATURE_SECOND = 0xAA;
/** Distance from the first to the second signature byte. */
constexpr size_t BPB_SIGNATURE_SECOND_STEP = 1;
/** Largest sectors per cluster accepted (64 KiB clusters). */
constexpr uint32_t MAX_CLUSTER_SECTORS = 128;
/** Largest number of FAT copies accepted. */
constexpr uint32_t MAX_FAT_COUNT = 4;
/** Fewest clusters a FAT32 volume can have; fewer means FAT12 or FAT16. */
constexpr uint32_t MIN_FAT32_CLUSTERS = 65525;
/** Cluster count at or above which the volume is refused (it would collide with the reserved FAT values). */
constexpr uint32_t MAX_FAT32_CLUSTERS = 0x0FFFFFF5;
/** Number of the first data cluster; clusters 0 and 1 are reserved. */
constexpr uint32_t FIRST_DATA_CLUSTER = 2;

/** Mask of the 28 bits of a FAT entry that hold the value. */
constexpr uint32_t FAT_VALUE_MASK = 0x0FFFFFFF;
/** Smallest value that marks the end of a cluster chain. */
constexpr uint32_t FAT_END_OF_CHAIN_MIN = 0x0FFFFFF8;
/** Value that marks a bad cluster. */
constexpr uint32_t FAT_BAD_CLUSTER = 0x0FFFFFF7;
/** Value written to end a chain. */
constexpr uint32_t FAT_END_OF_CHAIN = 0x0FFFFFFF;
/** Value of a free cluster. */
constexpr uint32_t FAT_FREE = 0;
/** Index of the FAT entry that carries the volume state bits. */
constexpr uint32_t FAT_STATE_ENTRY = 1;
/** Bit of FAT entry 1 that is set while the volume is cleanly unmounted. */
constexpr uint32_t FAT_CLEAN_SHUTDOWN_BIT = 0x08000000;
/** The 4 upper bits of a FAT entry, which must be preserved when writing. */
constexpr uint32_t FAT_RESERVED_MASK = 0xF0000000;

/** Signature at the start of the FSInfo sector. */
constexpr uint32_t FSINFO_LEAD_SIGNATURE = 0x41615252;
/** Signature before the free cluster fields. */
constexpr uint32_t FSINFO_STRUCT_SIGNATURE = 0x61417272;
/** Signature at the end of the FSInfo sector. */
constexpr uint32_t FSINFO_TRAIL_SIGNATURE = 0xAA550000;
/** Offset of the lead signature. */
constexpr size_t FSINFO_LEAD_OFFSET = 0;
/** Offset of the structure signature. */
constexpr size_t FSINFO_STRUCT_OFFSET = 484;
/** Offset of the free cluster count (32 bit). */
constexpr size_t FSINFO_FREE_OFFSET = 488;
/** Offset of the hint for the next free cluster (32 bit). */
constexpr size_t FSINFO_NEXT_OFFSET = 492;
/** Offset of the trail signature. */
constexpr size_t FSINFO_TRAIL_OFFSET = 508;
/** Value of an FSInfo field that is not known. */
constexpr uint32_t FSINFO_UNKNOWN = 0xFFFFFFFF;

/** First name byte of an entry that ends the directory: no entry after it is used. */
constexpr uint8_t DIRECTORY_END = 0x00;
/** First name byte of a deleted entry. */
constexpr uint8_t DIRECTORY_DELETED = 0xE5;
/** First name byte that stands for a real 0xE5 character. */
constexpr uint8_t DIRECTORY_KANJI_ESCAPE = 0x05;
/** Entry field: 11 byte short name (8 base plus 3 extension, space padded). */
constexpr size_t ENTRY_NAME_OFFSET = 0;
/** Bytes of the base part of a short name. */
constexpr size_t ENTRY_BASE_BYTES = 8;
/** Bytes of the extension of a short name. */
constexpr size_t ENTRY_EXTENSION_BYTES = 3;
/** Bytes of a whole short name (11). */
constexpr size_t ENTRY_SHORT_NAME_BYTES = ENTRY_BASE_BYTES + ENTRY_EXTENSION_BYTES;
/** Entry field: attribute bits (8 bit). */
constexpr size_t ENTRY_ATTRIBUTES_OFFSET = 11;
/** Entry field: lowercase flags for the base and the extension (8 bit). */
constexpr size_t ENTRY_CASE_OFFSET = 12;
/** Entry field: creation time (16 bit, FAT format). */
constexpr size_t ENTRY_CREATE_TIME_OFFSET = 14;
/** Entry field: creation date (16 bit, FAT format). */
constexpr size_t ENTRY_CREATE_DATE_OFFSET = 16;
/** Entry field: last access date (16 bit, FAT format). */
constexpr size_t ENTRY_ACCESS_DATE_OFFSET = 18;
/** Entry field: high 16 bits of the first cluster. */
constexpr size_t ENTRY_CLUSTER_HIGH_OFFSET = 20;
/** Entry field: last write time (16 bit, FAT format). */
constexpr size_t ENTRY_WRITE_TIME_OFFSET = 22;
/** Entry field: last write date (16 bit, FAT format). */
constexpr size_t ENTRY_WRITE_DATE_OFFSET = 24;
/** Entry field: low 16 bits of the first cluster. */
constexpr size_t ENTRY_CLUSTER_LOW_OFFSET = 26;
/** Entry field: file size in bytes (32 bit); 0 for directories. */
constexpr size_t ENTRY_SIZE_OFFSET = 28;
/** Default date written when no clock is available: 1980-01-01. */
constexpr uint16_t FAT_EPOCH_DATE = 0x0021;
/** Default time written when no clock is available: 00:00:00. */
constexpr uint16_t FAT_EPOCH_TIME = 0;
/** Attribute: the file changed since the last backup; set on new files. */
constexpr uint8_t ATTRIBUTE_ARCHIVE = 0x20;
/** Punctuation allowed in a short name besides upper case letters and digits. */
constexpr char SHORT_NAME_SYMBOLS[] = "$%'-_@~`!(){}^#&";
/** Longest short name as text: base, a dot and the extension (12). */
constexpr size_t MAX_SHORT_NAME_BYTES = ENTRY_BASE_BYTES + 1 + ENTRY_EXTENSION_BYTES;
/** Lowest decimal digit. */
constexpr char DIGIT_FIRST = '0';
/** Highest decimal digit. */
constexpr char DIGIT_LAST = '9';
/** Case flag: the base part is shown in lower case. */
constexpr uint8_t CASE_LOWER_BASE = 0x08;
/** Case flag: the extension is shown in lower case. */
constexpr uint8_t CASE_LOWER_EXTENSION = 0x10;
/** Attribute: the entry is the volume label, not a file. */
constexpr uint8_t ATTRIBUTE_VOLUME_LABEL = 0x08;
/** Attribute: the entry is a directory. */
constexpr uint8_t ATTRIBUTE_DIRECTORY = 0x10;
/** Attribute value that marks a long file name entry (read-only, hidden, system and volume label together). */
constexpr uint8_t ATTRIBUTE_LONG_NAME = 0x0F;
/** Mask applied to the attributes before comparing with `ATTRIBUTE_LONG_NAME`. */
constexpr uint8_t ATTRIBUTE_LONG_NAME_MASK = 0x3F;
/** Padding of short names. */
constexpr char SPACE = ' ';
/** Separator of base and extension. */
constexpr char DOT = '.';
/** Replacement for an unpaired UTF-16 surrogate. */
constexpr char UNKNOWN_CHARACTER = '?';

/** Flag in the sequence byte of the last (highest numbered, first stored) long name entry. */
constexpr uint8_t LONG_LAST_FLAG = 0x40;
/** Mask of the sequence number in the first byte of a long name entry. */
constexpr uint8_t LONG_INDEX_MASK = 0x1F;
/** Long entry field: checksum of the short name it belongs to (8 bit). */
constexpr size_t LONG_CHECKSUM_OFFSET = 13;
/** UTF-16 units stored in one long name entry. */
constexpr size_t LONG_CHARS_PER_ENTRY = 13;
/** Most long name entries per file (the limit of the format). */
constexpr size_t LONG_MAX_ENTRIES = 20;
/** Bytes of one UTF-16 unit. */
constexpr size_t LONG_CHAR_BYTES = 2;
/** Offset of the first group of 5 characters in a long entry. */
constexpr size_t LONG_FIRST_GROUP_OFFSET = 1;
/** Characters in the first group. */
constexpr size_t LONG_FIRST_GROUP_CHARS = 5;
/** Offset of the second group of 6 characters. */
constexpr size_t LONG_SECOND_GROUP_OFFSET = 14;
/** Characters in the second group. */
constexpr size_t LONG_SECOND_GROUP_CHARS = 6;
/** Offset of the third group of 2 characters. */
constexpr size_t LONG_THIRD_GROUP_OFFSET = 28;
/** Characters in the third group. */
constexpr size_t LONG_THIRD_GROUP_CHARS = 2;
/** Code points below this take one UTF-8 byte. */
constexpr uint32_t UTF8_ONE_BYTE_LIMIT = 0x80;
/** Code points below this take at most two UTF-8 bytes. */
constexpr uint32_t UTF8_TWO_BYTE_LIMIT = 0x800;
/** Code points below this take at most three UTF-8 bytes. */
constexpr uint32_t UTF8_THREE_BYTE_LIMIT = 0x10000;
/** Lead byte marker of a two byte sequence. */
constexpr uint32_t UTF8_TWO_BYTE_MARK = 0xC0;
/** Lead byte marker of a three byte sequence. */
constexpr uint32_t UTF8_THREE_BYTE_MARK = 0xE0;
/** Lead byte marker of a four byte sequence. */
constexpr uint32_t UTF8_FOUR_BYTE_MARK = 0xF0;
/** Marker of a continuation byte. */
constexpr uint32_t UTF8_CONTINUATION_MARK = 0x80;
/** Payload bits of a continuation byte. */
constexpr uint32_t UTF8_PAYLOAD_MASK = 0x3F;
/** Payload bits per continuation byte. */
constexpr unsigned UTF8_PAYLOAD_BITS = 6;
/** First UTF-16 high surrogate. */
constexpr uint32_t HIGH_SURROGATE_FIRST = 0xD800;
/** Last UTF-16 high surrogate. */
constexpr uint32_t HIGH_SURROGATE_LAST = 0xDBFF;
/** First UTF-16 low surrogate. */
constexpr uint32_t LOW_SURROGATE_FIRST = 0xDC00;
/** Last UTF-16 low surrogate. */
constexpr uint32_t LOW_SURROGATE_LAST = 0xDFFF;
/** First code point that needs a surrogate pair. */
constexpr uint32_t SUPPLEMENTARY_BASE = 0x10000;
/** Payload bits in each surrogate. */
constexpr unsigned SURROGATE_BITS = 10;
/** UTF-16 unit that ends a long name when it does not fill the entries. */
constexpr uint16_t LONG_NAME_END = 0x0000;
/** Most UTF-16 units in a name (260). */
constexpr size_t MAX_NAME_CHARS = LONG_MAX_ENTRIES * LONG_CHARS_PER_ENTRY;
/** Most UTF-8 bytes produced by one UTF-16 unit. */
constexpr size_t UTF8_MAX_UNIT_BYTES = 3;
/** Size of a buffer that holds any decoded name, with the terminator. */
constexpr size_t MAX_NAME_BYTES = MAX_NAME_CHARS * UTF8_MAX_UNIT_BYTES + 1;
/** Right rotate step of the short name checksum. */
constexpr unsigned CHECKSUM_RIGHT_SHIFT = 1;
/** Left part of the rotate of the short name checksum. */
constexpr unsigned CHECKSUM_LEFT_SHIFT = 7;
/** Mask that keeps the checksum in 8 bits. */
constexpr uint8_t CHECKSUM_BYTE_MASK = 0xFF;
/** First upper case letter. */
constexpr char LETTER_UPPER_A = 'A';
/** Last upper case letter. */
constexpr char LETTER_UPPER_Z = 'Z';
/** First lower case letter. */
constexpr char LETTER_LOWER_A = 'a';
/** Last lower case letter. */
constexpr char LETTER_LOWER_Z = 'z';
/** Mask of the low 16 bits. */
constexpr uint32_t LOW_HALF_MASK = 0xFFFF;
/** Distance between a lower case and an upper case ASCII letter. */
constexpr char CASE_DISTANCE = LETTER_LOWER_A - LETTER_UPPER_A;
/** Sequence number of the long entry nearest to the short entry. */
constexpr size_t LONG_FIRST_INDEX = 1;
/** Index of the second byte of a short name, used to recognise `.` and `..`. */
constexpr size_t DOT_SECOND_BYTE = 1;
/** Index where the padding of the `..` short name starts. */
constexpr size_t DOT_PADDING_START = 2;
/** Unit that fills the unused space after the end of a long name. */
constexpr uint16_t LONG_PADDING_UNIT = 0xFFFF;
/** ASCII characters that a long name must not contain. */
constexpr char FORBIDDEN_LONG_CHARACTERS[] = "\"*/:<>?\\|";
/** Control character DEL, forbidden in names. */
constexpr char DELETE_CHARACTER = 0x7F;
/** Lowest character allowed in a name; lower code points are forbidden. */
constexpr char FIRST_PRINTABLE = 0x20;
/** Marker of generated short name aliases (`NAME~1`). */
constexpr char TILDE = '~';
/** Replacement for characters a short name cannot hold. */
constexpr char UNDERSCORE = '_';
/** Mask to test for a continuation byte. */
constexpr uint32_t UTF8_CONTINUATION_MASK = 0xC0;
/** Mask to test for a two byte lead. */
constexpr uint32_t UTF8_TWO_LEAD_MASK = 0xE0;
/** Mask to test for a three byte lead. */
constexpr uint32_t UTF8_THREE_LEAD_MASK = 0xF0;
/** Mask to test for a four byte lead. */
constexpr uint32_t UTF8_FOUR_LEAD_MASK = 0xF8;
/** Payload bits of a two byte lead. */
constexpr uint32_t UTF8_TWO_PAYLOAD_MASK = 0x1F;
/** Payload bits of a three byte lead. */
constexpr uint32_t UTF8_THREE_PAYLOAD_MASK = 0x0F;
/** Payload bits of a four byte lead. */
constexpr uint32_t UTF8_FOUR_PAYLOAD_MASK = 0x07;
/** Largest Unicode code point. */
constexpr uint32_t MAX_CODE_POINT = 0x10FFFF;
/** Mask of the payload of a surrogate. */
constexpr uint32_t SURROGATE_MASK = 0x3FF;
/** Base of the numeric tail of generated aliases. */
constexpr uint32_t DECIMAL_RADIX = 10;
/** Most digits in the `~N` tail of an alias. */
constexpr uint32_t MAX_ALIAS_TAIL_DIGITS = 6;
/** Largest tail number of an alias. */
constexpr uint32_t MAX_ALIAS_TAIL = 999999;

/** Shift that places a byte at bits 8 to 15. */
constexpr unsigned SHIFT_8 = 8;
/** Shift that places a value at bits 16 to 31. */
constexpr unsigned SHIFT_16 = 16;
/** Shift that places a byte at bits 24 to 31. */
constexpr unsigned SHIFT_24 = 24;
/** Index of the third byte of a 32 bit field. */
constexpr size_t THIRD_BYTE = 2;
/** Index of the fourth byte of a 32 bit field. */
constexpr size_t FOURTH_BYTE = 3;

/** Reason why `parseBpb` refused a volume. */
enum class BpbError : uint32_t {
	/** The boot sector is valid. */
	NONE = 0,
	/** The boot signature 0x55 0xAA is missing. */
	BAD_SIGNATURE,
	/** Bytes per sector is not 512. */
	BAD_SECTOR_SIZE,
	/** Sectors per cluster is not a power of two up to 128. */
	BAD_CLUSTER_SIZE,
	/** Counts, sizes or the root cluster are inconsistent or exceed the partition. */
	BAD_LAYOUT,
	/** The volume is FAT12 or FAT16 (fixed root, 16 bit FAT size or too few clusters). */
	NOT_FAT32,
};

/** Decoded boot sector with the values needed to address the volume. Sectors are relative to the partition. */
struct Bpb {
	/** Sectors in a cluster (a power of two, 1 to 128). */
	uint32_t sectorsPerCluster;
	/** Sectors before the first FAT; the first FAT starts at this sector. */
	uint32_t reservedSectors;
	/** Number of FAT copies (1 to 4); writes must update all of them. */
	uint32_t fatCount;
	/** Sectors in one FAT. */
	uint32_t fatSectors;
	/** Sector of the FSInfo structure; 0 or 0xFFFF when there is none. */
	uint32_t fsInfoSector;
	/** First cluster of the root directory. */
	uint32_t rootCluster;
	/** Number of data clusters, numbered from 2. */
	uint32_t clusterCount;
	/** First sector of the data area (cluster 2). */
	uint32_t dataStart;
	/** Sectors in the volume. */
	uint32_t totalSectors;

	/**
	 * @brief Size of a cluster.
	 * @return Bytes per cluster.
	 */
	uint32_t clusterBytes() const { return sectorsPerCluster * uint32_t(SECTOR_BYTES); }

	/**
	 * @brief Tells whether a cluster number is inside the data area.
	 * @param cluster Cluster number.
	 * @return True when it is between 2 and `clusterCount + 1`.
	 */
	bool isDataCluster(uint32_t cluster) const {
		return cluster >= FIRST_DATA_CLUSTER && cluster - FIRST_DATA_CLUSTER < clusterCount;
	}

	/**
	 * @brief First sector of a data cluster.
	 * @param cluster A cluster number accepted by `isDataCluster`.
	 * @return Sector relative to the partition.
	 */
	uint32_t clusterSector(uint32_t cluster) const {
		return dataStart + (cluster - FIRST_DATA_CLUSTER) * sectorsPerCluster;
	}
};

/** A decoded directory entry (a file or a directory). */
struct DirEntry {
	/** Name in UTF-8, null terminated: the long name when valid, otherwise the short name. */
	char name[MAX_NAME_BYTES];
	/** Attribute bits. */
	uint8_t attributes;
	/** First cluster of the data; 0 for an empty file. */
	uint32_t cluster;
	/** File size in bytes; 0 for a directory. */
	uint32_t size;

	/**
	 * @brief Tells whether the entry is a directory.
	 * @return True when the directory attribute is set.
	 */
	bool isDirectory() const { return (attributes & ATTRIBUTE_DIRECTORY) != 0; }
};

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
 * @return True for 1, 2, 4 and so on; false for 0.
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

/**
 * @brief Converts an ASCII upper case letter to lower case.
 * @param character Any character.
 * @return The lower case letter, or the character unchanged when it is not an upper case letter.
 */
inline char lower(char character) {
	return character >= LETTER_UPPER_A && character <= LETTER_UPPER_Z ? char(character + CASE_DISTANCE) : character;
}

} // namespace detail

/**
 * @brief Decodes and validates the boot sector of a FAT32 volume.
 *
 * Rejects other sector sizes, odd cluster sizes, FAT12 and FAT16 layouts, a non zero file system version, a FAT too small
 * for its clusters, and a root cluster outside the data area. The volume must fit in the partition.
 * @param sector Buffer of 512 bytes with sector 0 of the partition.
 * @param partitionSectors Size of the partition in sectors.
 * @param bpb Receives the decoded values on success.
 * @return `BpbError::NONE` or the first problem found.
 */
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

/**
 * @brief Reads one FAT entry.
 * @param fatSector A 512 byte sector of the FAT.
 * @param indexInSector Entry index inside the sector, 0 to 127.
 * @return The 28 bit value (the reserved upper bits are removed).
 */
inline uint32_t fatValue(const uint8_t *fatSector, uint32_t indexInSector) {
	return detail::readLe32(fatSector + size_t(indexInSector) * FAT_ENTRY_BYTES) &
		   FAT_VALUE_MASK;
}

/**
 * @brief Tells whether a FAT value ends a chain.
 * @param value Value from `fatValue`.
 * @return True for 0x0FFFFFF8 and above.
 */
inline bool isEndOfChain(uint32_t value) { return value >= FAT_END_OF_CHAIN_MIN; }

/**
 * @brief Writes one FAT entry, keeping the reserved upper 4 bits.
 * @param fatSector A 512 byte sector of the FAT.
 * @param indexInSector Entry index inside the sector, 0 to 127.
 * @param value New 28 bit value; upper bits are ignored.
 */
inline void setFatValue(uint8_t *fatSector, uint32_t indexInSector, uint32_t value) {
	uint8_t *entry = fatSector + size_t(indexInSector) * FAT_ENTRY_BYTES;
	uint32_t kept = detail::readLe32(entry) & FAT_RESERVED_MASK;
	detail::writeLe32(entry, kept | (value & FAT_VALUE_MASK));
}

/**
 * @brief Checks the three signatures of an FSInfo sector.
 * @param sector 512 bytes of the FSInfo sector.
 * @return True when all signatures match.
 */
inline bool validFsInfo(const uint8_t *sector) {
	return detail::readLe32(sector + FSINFO_LEAD_OFFSET) == FSINFO_LEAD_SIGNATURE &&
		   detail::readLe32(sector + FSINFO_STRUCT_OFFSET) == FSINFO_STRUCT_SIGNATURE &&
		   detail::readLe32(sector + FSINFO_TRAIL_OFFSET) == FSINFO_TRAIL_SIGNATURE;
}

/**
 * @brief Reads the free cluster count of the FSInfo sector.
 * @param sector A valid FSInfo sector.
 * @return The count, or `FSINFO_UNKNOWN`. It is a hint and is not trusted.
 */
inline uint32_t fsInfoFree(const uint8_t *sector) {
	return detail::readLe32(sector + FSINFO_FREE_OFFSET);
}

/**
 * @brief Reads the next free cluster hint of the FSInfo sector.
 * @param sector A valid FSInfo sector.
 * @return The cluster number, or `FSINFO_UNKNOWN`.
 */
inline uint32_t fsInfoNext(const uint8_t *sector) {
	return detail::readLe32(sector + FSINFO_NEXT_OFFSET);
}

/**
 * @brief Updates the free cluster hints in an FSInfo sector.
 * @param sector A valid FSInfo sector, changed in place.
 * @param freeClusters New free cluster count.
 * @param nextFree New hint for the next free cluster.
 */
inline void storeFsInfo(uint8_t *sector, uint32_t freeClusters, uint32_t nextFree) {
	detail::writeLe32(sector + FSINFO_FREE_OFFSET, freeClusters);
	detail::writeLe32(sector + FSINFO_NEXT_OFFSET, nextFree);
}

/**
 * @brief Tells whether a character may appear in a short name.
 * @param character Character, already converted to upper case.
 * @return True for upper case letters, digits and the characters of `SHORT_NAME_SYMBOLS`.
 */
inline bool isShortNameCharacter(char character) {
	if (character >= DIGIT_FIRST && character <= DIGIT_LAST) return true;
	if (character >= LETTER_UPPER_A && character <= LETTER_UPPER_Z) return true;
	for (size_t index = 0; SHORT_NAME_SYMBOLS[index] != '\0'; index++)
		if (character == SHORT_NAME_SYMBOLS[index]) return true;
	return false;
}

/**
 * @brief Encodes the base or the extension of a short name.
 * @param part Characters of the part.
 * @param length Number of characters.
 * @param target Destination, at least `length` bytes, written in upper case.
 * @param lowered Receives true when the part was all lower case.
 * @return False when a character is not allowed or the part mixes upper and lower case.
 */
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

/**
 * @brief Encodes a file name as an 11 byte short name when it fits exactly.
 * @param name File name in ASCII.
 * @param length Length of the name.
 * @param shortName Receives 11 bytes, space padded.
 * @param caseFlags Receives the `CASE_LOWER_*` flags that restore the original case.
 * @return False when the name needs a long name entry: empty, too long, bad characters, mixed case inside a part, or a trailing dot.
 */
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

/**
 * @brief Stores the first cluster and the size in a directory entry.
 * @param raw The 32 byte entry, changed in place.
 * @param cluster First cluster.
 * @param size File size in bytes.
 */
inline void patchEntryLocation(uint8_t *raw, uint32_t cluster, uint32_t size) {
	detail::writeLe16(raw + ENTRY_CLUSTER_HIGH_OFFSET, cluster >> SHIFT_16);
	detail::writeLe16(raw + ENTRY_CLUSTER_LOW_OFFSET, cluster & LOW_HALF_MASK);
	detail::writeLe32(raw + ENTRY_SIZE_OFFSET, size);
}

/**
 * @brief Sets the access and write date and time of an entry.
 * @param raw The 32 byte entry, changed in place.
 * @param date FAT date (see `datetime::toFatDate`).
 * @param time FAT time (see `datetime::toFatTime`).
 */
inline void touchEntry(uint8_t *raw, uint16_t date, uint16_t time) {
	detail::writeLe16(raw + ENTRY_ACCESS_DATE_OFFSET, date);
	detail::writeLe16(raw + ENTRY_WRITE_TIME_OFFSET, time);
	detail::writeLe16(raw + ENTRY_WRITE_DATE_OFFSET, date);
}

/**
 * @brief Builds a complete directory entry.
 * @param raw Destination of 32 bytes, cleared first.
 * @param shortName 11 byte short name.
 * @param caseFlags `CASE_LOWER_*` flags.
 * @param attributes Attribute bits.
 * @param cluster First cluster, 0 for an empty file.
 * @param size File size in bytes.
 * @param date FAT date used for creation, access and write; defaults to 1980-01-01.
 * @param time FAT time used for creation and write; defaults to 00:00:00.
 */
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

/**
 * @brief Compares two names ignoring ASCII case, as FAT does.
 * @param first Null terminated name.
 * @param second Null terminated name.
 * @return True when equal.
 */
inline bool namesEqual(const char *first, const char *second) {
	size_t index = 0;
	for (; first[index] != '\0' && second[index] != '\0'; index++)
		if (detail::lower(first[index]) != detail::lower(second[index])) return false;
	return first[index] == second[index];
}

/**
 * @brief Checksum that ties long name entries to their short entry.
 * @param shortName 11 byte short name.
 * @return The 8 bit checksum (rotate right and add).
 */
inline uint8_t shortNameChecksum(const uint8_t *shortName) {
	uint8_t sum = 0;
	for (size_t index = 0; index < ENTRY_SHORT_NAME_BYTES; index++)
		sum = uint8_t(((sum >> CHECKSUM_RIGHT_SHIFT) | (sum << CHECKSUM_LEFT_SHIFT)) &
					  CHECKSUM_BYTE_MASK) +
			  shortName[index];
	return sum;
}

/** How a new file name will be stored. */
enum class NamePlan : uint32_t {
	/** A plain short entry; the case is exact or restored by the case flags. */
	SHORT = 0,
	/** A short entry that fits in upper case but the name has mixed case; long name entries are written to keep the case. */
	CASED = 1,
	/** The name does not fit; a generated alias (`NAME~1`) and long name entries are needed. */
	LOSSY = 2,
};

/**
 * @brief Decides how to store a name.
 * @param name File name in ASCII (callers decode UTF-8 separately).
 * @param length Length of the name.
 * @param shortName Receives the short name for `SHORT` and `CASED`; not meaningful for `LOSSY`.
 * @param caseFlags Receives the case flags for `SHORT`, and 0 for `CASED`.
 * @return The plan.
 */
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

/**
 * @brief Builds a numeric tail alias such as `LONGNA~1.TXT`.
 *
 * Leading dots, spaces and non ASCII bytes are dropped, letters are upper cased and other forbidden characters become underscores.
 * @param name Original name in UTF-8.
 * @param length Length of the name.
 * @param tail Tail number, 1 to `MAX_ALIAS_TAIL`.
 * @param shortName Receives the 11 byte alias.
 */
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

/**
 * @brief Tells whether a code point may not appear in a name.
 * @param code Unicode code point.
 * @return True for control characters, DEL and the characters of `FORBIDDEN_LONG_CHARACTERS`.
 */
inline bool isForbiddenInLongName(uint32_t code) {
	if (code < uint32_t(FIRST_PRINTABLE) || code == uint32_t(DELETE_CHARACTER)) return true;
	for (size_t index = 0; FORBIDDEN_LONG_CHARACTERS[index] != '\0'; index++)
		if (code == uint32_t(FORBIDDEN_LONG_CHARACTERS[index])) return true;
	return false;
}

/**
 * @brief Decodes a UTF-8 name into UTF-16 units, strictly.
 *
 * Rejects overlong forms, surrogates, code points above U+10FFFF, truncated sequences, forbidden characters, an empty name, and a name that ends with a dot or a space.
 * @param text UTF-8 bytes.
 * @param length Number of bytes.
 * @param units Destination of UTF-16 units.
 * @param capacity Size of `units` in units.
 * @param count Receives the number of units written.
 * @return True when the whole name is valid and fits.
 */
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

/**
 * @brief Byte offset of one of the 13 character slots of a long name entry.
 * @param slot Slot number, 0 to 12.
 * @return Offset inside the 32 byte entry.
 */
inline size_t longSlotOffset(size_t slot) {
	if (slot < LONG_FIRST_GROUP_CHARS) return LONG_FIRST_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
	slot -= LONG_FIRST_GROUP_CHARS;
	if (slot < LONG_SECOND_GROUP_CHARS)
		return LONG_SECOND_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
	slot -= LONG_SECOND_GROUP_CHARS;
	return LONG_THIRD_GROUP_OFFSET + slot * LONG_CHAR_BYTES;
}

/**
 * @brief Number of long name entries needed for a name.
 * @param unitCount Name length in UTF-16 units.
 * @return `unitCount` divided by 13, rounded up.
 */
inline size_t longEntryCount(size_t unitCount) {
	return (unitCount + LONG_CHARS_PER_ENTRY - 1) / LONG_CHARS_PER_ENTRY;
}

/**
 * @brief Builds one long name entry.
 * @param raw Destination of 32 bytes, cleared first.
 * @param sequence Sequence number, 1 for the entry nearest to the short entry.
 * @param last True for the highest numbered entry, which is stored first.
 * @param checksum `shortNameChecksum` of the short entry.
 * @param units All UTF-16 units of the name.
 * @param unitCount Number of units; the slot after the end gets a terminator and the rest padding.
 */
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

/**
 * @brief Builds the short name of the `.` or `..` entry.
 * @param parent True for `..`, false for `.`.
 * @param shortName Receives 11 bytes.
 */
inline void dotShortName(bool parent, uint8_t *shortName) {
	for (size_t index = 0; index < ENTRY_SHORT_NAME_BYTES; index++)
		shortName[index] = uint8_t(SPACE);
	shortName[ENTRY_NAME_OFFSET] = uint8_t(DOT);
	if (parent) shortName[DOT_SECOND_BYTE] = uint8_t(DOT);
}

/**
 * Stateful decoder of the 32 byte entries of a directory, in order.
 *
 * It collects long name entries until the short entry that they belong to (checked with the sequence numbers and the checksum) and
 * returns one decoded entry per file. Deleted entries, the volume label and `.` and `..` are skipped. Not thread safe.
 */
class DirectoryParser {
  public:
	/** Outcome of `DirectoryParser::feed`. */
	enum class Result : uint32_t {
		/** The entry was consumed and produced nothing (long name part, deleted, label, dot entry). */
		SKIP = 0,
		/** A file or directory was decoded into the output entry. */
		ENTRY = 1,
		/** The directory ends here; later entries must not be read. */
		END = 2,
	};

	/** Forgets any partially read long name; call it when starting a new directory. */
	void reset() { clearLongName(); }

	/**
	 * @brief Processes the next directory entry.
	 * @param raw The 32 byte raw entry.
	 * @param entry Receives the decoded entry when `Result::ENTRY` is returned.
	 * @return What the caller must do with the entry.
	 */
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
	/** UTF-16 units of the long name being collected. */
	uint16_t longName[MAX_NAME_CHARS + 1];
	/** Sequence number expected next while collecting; 0 when no long name is pending, 1 when it is complete. */
	size_t nextIndex = 0;
	/** Capacity in units of the long name being collected (entries times 13). */
	size_t longLength = 0;
	/** Short name checksum announced by the long entries. */
	uint8_t checksum = 0;

	/** Drops the pending long name. */
	void clearLongName() {
		nextIndex = 0;
		longLength = 0;
	}

	/**
	 * @brief Tells whether a short entry is `.` or `..`.
	 * @param raw The 32 byte entry.
	 * @return True for the names `.` and `..`.
	 */
	static bool isDotName(const uint8_t *raw) {
		if (raw[DOT_SECOND_BYTE] == uint8_t(SPACE)) return true;
		if (raw[DOT_SECOND_BYTE] != uint8_t(DOT)) return false;
		for (size_t index = DOT_PADDING_START; index < ENTRY_SHORT_NAME_BYTES; index++)
			if (raw[index] != uint8_t(SPACE)) return false;
		return true;
	}

	/**
	 * @brief Adds a long name entry to the pending name, or drops the pending name when the order or the checksum is wrong.
	 * @param raw The 32 byte long name entry.
	 */
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

	/**
	 * @brief Copies a group of UTF-16 units out of a long name entry.
	 * @param source First unit in the entry.
	 * @param chars Number of units.
	 * @param target Destination.
	 */
	static void copyGroup(const uint8_t *source, size_t chars, uint16_t *target) {
		for (size_t index = 0; index < chars; index++)
			target[index] = detail::readLe16(source + index * LONG_CHAR_BYTES);
	}

	/**
	 * @brief Encodes a code point as UTF-8.
	 * @param code Code point, at most U+10FFFF.
	 * @param target Destination of at least 4 bytes.
	 * @return Bytes written, 1 to 4.
	 */
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

	/**
	 * @brief Converts the pending long name to UTF-8 in an entry, replacing unpaired surrogates with a question mark.
	 * @param entry Receives the null terminated name.
	 */
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

	/**
	 * @brief Builds the text of a short name (`NAME.EXT`), applying the case flags.
	 * @param raw The 32 byte short entry.
	 * @param entry Receives the null terminated name.
	 */
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
