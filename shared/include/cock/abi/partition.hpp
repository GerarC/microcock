/**
 * @file partition.hpp
 * @brief Parsers of the MBR and GPT partition tables, with the CRC32 that GPT needs.
 *
 * Pure functions over sector buffers: no I/O and no dependencies, so they are used by the block driver and tested on the host. All
 * on-disk integers are little endian. Sector numbers are 64 bit. A parser never trusts the table: every entry
 * must lie inside the disk, and a bad entry is skipped instead of failing the whole table.
 */
#ifndef SHARED_COCK_ABI_PARTITION_HPP
#define SHARED_COCK_ABI_PARTITION_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::partition {

/** Size of a sector in bytes. */
constexpr size_t SECTOR_BYTES = 512;
/** Most partitions kept in a `Table`; further entries are ignored. */
constexpr size_t MAX_PARTITIONS = 16;

/** Offset in sector 0 of the first byte of the boot signature. */
constexpr size_t MBR_SIGNATURE_OFFSET = 510;
/** Value of the byte at `MBR_SIGNATURE_OFFSET`. */
constexpr uint8_t MBR_SIGNATURE_FIRST = 0x55;
/** Value of the byte after the first signature byte. */
constexpr uint8_t MBR_SIGNATURE_SECOND = 0xAA;
/** Distance in bytes from the first to the second signature byte. */
constexpr size_t MBR_SIGNATURE_SECOND_STEP = 1;
/** Offset of the first of the four partition entries. */
constexpr size_t MBR_TABLE_OFFSET = 446;
/** Number of primary entries in an MBR. */
constexpr size_t MBR_ENTRY_COUNT = 4;
/** Size of one MBR entry in bytes. */
constexpr size_t MBR_ENTRY_BYTES = 16;
/** Offset of the type byte inside an entry. */
constexpr size_t MBR_TYPE_OFFSET = 4;
/** Offset of the 32 bit first sector (LBA) inside an entry. */
constexpr size_t MBR_FIRST_SECTOR_OFFSET = 8;
/** Offset of the 32 bit sector count inside an entry. */
constexpr size_t MBR_SECTOR_COUNT_OFFSET = 12;
/** Type of an unused entry. */
constexpr uint8_t MBR_TYPE_EMPTY = 0x00;
/** Type of the protective entry that marks a GPT disk. */
constexpr uint8_t MBR_TYPE_PROTECTIVE = 0xEE;
/** Extended partition (CHS); not parsed, skipped. */
constexpr uint8_t MBR_TYPE_EXTENDED_CHS = 0x05;
/** Extended partition (LBA); not parsed, skipped. */
constexpr uint8_t MBR_TYPE_EXTENDED_LBA = 0x0F;
/** Linux extended partition; not parsed, skipped. */
constexpr uint8_t MBR_TYPE_EXTENDED_LINUX = 0x85;

/** Sector (LBA) of the primary GPT header; the header must also name itself as LBA 1. */
constexpr size_t GPT_HEADER_SECTOR = 1;
/** Smallest valid header size. */
constexpr size_t GPT_MIN_HEADER_BYTES = 92;
/** Length of the header signature. */
constexpr size_t GPT_SIGNATURE_BYTES = 8;
/** Signature at the start of the header (plus the C terminator). */
constexpr char GPT_SIGNATURE[GPT_SIGNATURE_BYTES + 1] = "EFI PART";
/** Offset of the 32 bit header size. */
constexpr size_t GPT_HEADER_SIZE_OFFSET = 12;
/** Offset of the 32 bit header CRC32 (computed with this field zeroed). */
constexpr size_t GPT_HEADER_CRC_OFFSET = 16;
/** Size of a CRC32 field. */
constexpr size_t GPT_CRC_BYTES = 4;
/** Offset of the 64 bit LBA where the header says it lives. */
constexpr size_t GPT_MY_LBA_OFFSET = 24;
/** Offset of the 64 bit first usable LBA. */
constexpr size_t GPT_FIRST_USABLE_OFFSET = 40;
/** Offset of the 64 bit last usable LBA. */
constexpr size_t GPT_LAST_USABLE_OFFSET = 48;
/** Offset of the 64 bit LBA of the entry array. */
constexpr size_t GPT_ENTRIES_LBA_OFFSET = 72;
/** Offset of the 32 bit number of entries. */
constexpr size_t GPT_ENTRY_COUNT_OFFSET = 80;
/** Offset of the 32 bit size of one entry. */
constexpr size_t GPT_ENTRY_BYTES_OFFSET = 84;
/** Offset of the 32 bit CRC32 of the entry array. */
constexpr size_t GPT_ENTRIES_CRC_OFFSET = 88;
/** Most entries accepted in the array. */
constexpr size_t GPT_MAX_ENTRIES = 128;
/** Only entry size accepted. */
constexpr size_t GPT_ENTRY_BYTES = 128;
/** Largest entry array in bytes (16 KiB); size of the buffer the caller must provide. */
constexpr size_t GPT_ENTRIES_MAX_BYTES = GPT_MAX_ENTRIES * GPT_ENTRY_BYTES;
/** Offset of the type GUID inside an entry; an all zero GUID marks an unused entry. */
constexpr size_t GPT_TYPE_GUID_OFFSET = 0;
/** Size of a GUID. */
constexpr size_t GPT_GUID_BYTES = 16;
/** Offset of the 64 bit first LBA inside an entry. */
constexpr size_t GPT_ENTRY_FIRST_OFFSET = 32;
/** Offset of the 64 bit last LBA (inclusive) inside an entry. */
constexpr size_t GPT_ENTRY_LAST_OFFSET = 40;

/** Reflected IEEE 802.3 polynomial. */
constexpr uint32_t CRC32_POLYNOMIAL = 0xEDB88320;
/** Initial value of the CRC register (the result is inverted at the end). */
constexpr uint32_t CRC32_INITIAL = 0xFFFFFFFF;
/** Mask of the bit that decides the XOR step. */
constexpr uint32_t CRC32_LOW_BIT = 1;
/** Bits the register moves per step. */
constexpr unsigned CRC32_SHIFT = 1;
/** Bits in a byte. */
constexpr unsigned BITS_PER_BYTE = 8;
/** Mask of one byte. */
constexpr unsigned BYTE_MASK = 0xFF;
/** Shift that places a byte at bits 16 to 23. */
constexpr unsigned SHIFT_16 = 16;
/** Shift that places a byte at bits 24 to 31. */
constexpr unsigned SHIFT_24 = 24;
/** Shift that places a 32 bit half in the top of a 64 bit value. */
constexpr unsigned SHIFT_32 = 32;
/** Index of the most significant byte of a 32 bit field. */
constexpr unsigned LAST_BYTE_INDEX = 3;

/** Kind of table found on a disk. */
enum class Scheme : uint32_t {
	/** No valid table. */
	NONE = 0,
	/** Legacy MBR table. */
	MBR = 1,
	/** GUID Partition Table. */
	GPT = 2,
};

/** Outcome of `parseMbr`. */
enum class MbrResult : uint32_t {
	/** The sector has no boot signature. */
	INVALID = 0,
	/** A valid MBR; the `Table` holds its primary partitions. */
	PRIMARY = 1,
	/** The MBR only protects a GPT disk; the caller must read the GPT header. */
	PROTECTIVE = 2,
};

/** One partition. */
struct Entry {
	/** First sector of the partition on the disk. */
	uint64_t firstSector;
	/** Length in sectors, at least 1; the partition ends inside the disk. */
	uint64_t sectorCount;
};

/** Partitions found on a disk. */
struct Table {
	/** Kind of table the entries came from. */
	Scheme scheme;
	/** Partitions in table order; only the first `count` are valid. */
	Entry entries[MAX_PARTITIONS];
	/** Number of valid entries, at most `MAX_PARTITIONS`. */
	size_t count;
};

/** Fields of a validated GPT header that are needed to read the entry array. */
struct GptHeader {
	/** First sector of the entry array, after the header and inside the disk. */
	uint64_t entriesSector;
	/** Number of entries, 1 to `GPT_MAX_ENTRIES`. */
	uint32_t entryCount;
	/** Expected CRC32 of the entry array. */
	uint32_t entriesCrc;
	/** First sector partitions may use. */
	uint64_t firstUsable;
	/** Last sector partitions may use, inside the disk. */
	uint64_t lastUsable;

	/**
	 * @brief Size of the entry array.
	 * @return `entryCount * GPT_ENTRY_BYTES` in bytes.
	 */
	size_t entriesBytes() const { return size_t(entryCount) * GPT_ENTRY_BYTES; }

	/**
	 * @brief Size of the entry array in whole sectors.
	 * @return `entriesBytes()` rounded up to sectors.
	 */
	size_t entriesSectors() const {
		return (entriesBytes() + SECTOR_BYTES - 1) / SECTOR_BYTES;
	}
};

namespace detail {

/**
 * @brief Reads a little endian 32 bit integer from unaligned bytes.
 * @param bytes At least 4 readable bytes.
 * @return The value.
 */
inline uint32_t readLe32(const uint8_t *bytes) {
	return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << BITS_PER_BYTE) |
		   (uint32_t(bytes[2]) << SHIFT_16) | (uint32_t(bytes[LAST_BYTE_INDEX]) << SHIFT_24);
}

/**
 * @brief Reads a little endian 64 bit integer from unaligned bytes.
 * @param bytes At least 8 readable bytes.
 * @return The value.
 */
inline uint64_t readLe64(const uint8_t *bytes) {
	return uint64_t(readLe32(bytes)) |
		   (uint64_t(readLe32(bytes + sizeof(uint32_t))) << SHIFT_32);
}

/**
 * @brief Tells whether an MBR type byte is an extended partition.
 * @param type MBR type byte.
 * @return True for 0x05, 0x0F and 0x85.
 */
inline bool isExtended(uint8_t type) {
	return type == MBR_TYPE_EXTENDED_CHS || type == MBR_TYPE_EXTENDED_LBA ||
		   type == MBR_TYPE_EXTENDED_LINUX;
}

/**
 * @brief Checks that a sector range lies inside the disk, without overflow.
 * @param first First sector.
 * @param count Length in sectors.
 * @param diskSectors Size of the disk in sectors.
 * @return True when `count` is not 0 and `first + count` does not pass `diskSectors`.
 */
inline bool fitsDisk(uint64_t first, uint64_t count, uint64_t diskSectors) {
	return count != 0 && first < diskSectors && count <= diskSectors - first;
}

/**
 * @brief Tells whether a byte range is all zero.
 * @param bytes Bytes to test.
 * @param length Number of bytes.
 * @return True when every byte is 0.
 */
inline bool isZero(const uint8_t *bytes, size_t length) {
	for (size_t index = 0; index < length; index++)
		if (bytes[index] != 0) return false;
	return true;
}

} // namespace detail

/**
 * @brief CRC32 (IEEE 802.3, as used by GPT).
 * @param bytes Data to check.
 * @param length Number of bytes.
 * @return The checksum.
 */
inline uint32_t crc32(const uint8_t *bytes, size_t length) {
	uint32_t crc = CRC32_INITIAL;
	for (size_t index = 0; index < length; index++) {
		crc ^= bytes[index];
		for (unsigned bit = 0; bit < BITS_PER_BYTE; bit++)
			crc = (crc & CRC32_LOW_BIT) != 0 ? (crc >> CRC32_SHIFT) ^ CRC32_POLYNOMIAL
											 : crc >> CRC32_SHIFT;
	}
	return ~crc;
}

/**
 * @brief Parses sector 0 as an MBR.
 *
 * Entries that are empty, extended, or outside the disk are skipped. Logical partitions inside an extended partition are not read.
 * @param sector Buffer of `SECTOR_BYTES` bytes with sector 0.
 * @param diskSectors Size of the disk in sectors, used to reject entries that do not fit.
 * @param table Receives the primary partitions; cleared first.
 * @return `INVALID` without signature, `PROTECTIVE` when an entry has type 0xEE (the table is left empty), otherwise `PRIMARY`.
 */
inline MbrResult parseMbr(const uint8_t *sector, uint64_t diskSectors, Table &table) {
	table.scheme = Scheme::NONE;
	table.count = 0;
	if (sector[MBR_SIGNATURE_OFFSET] != MBR_SIGNATURE_FIRST ||
		sector[MBR_SIGNATURE_OFFSET + MBR_SIGNATURE_SECOND_STEP] != MBR_SIGNATURE_SECOND)
		return MbrResult::INVALID;

	for (size_t slot = 0; slot < MBR_ENTRY_COUNT; slot++) {
		const uint8_t *entry = sector + MBR_TABLE_OFFSET + slot * MBR_ENTRY_BYTES;
		if (entry[MBR_TYPE_OFFSET] == MBR_TYPE_PROTECTIVE) return MbrResult::PROTECTIVE;
	}

	for (size_t slot = 0; slot < MBR_ENTRY_COUNT; slot++) {
		const uint8_t *entry = sector + MBR_TABLE_OFFSET + slot * MBR_ENTRY_BYTES;
		uint8_t type = entry[MBR_TYPE_OFFSET];
		if (type == MBR_TYPE_EMPTY || detail::isExtended(type)) continue;

		uint64_t first = detail::readLe32(entry + MBR_FIRST_SECTOR_OFFSET);
		uint64_t count = detail::readLe32(entry + MBR_SECTOR_COUNT_OFFSET);
		if (!detail::fitsDisk(first, count, diskSectors)) continue;
		if (table.count == MAX_PARTITIONS) break;
		table.entries[table.count++] = {first, count};
	}
	table.scheme = Scheme::MBR;
	return MbrResult::PRIMARY;
}

/**
 * @brief Validates the GPT header found in sector 1.
 *
 * Checks the signature, a header size from 92 to 512 bytes, the header CRC32, that the header names LBA 1 as its own,
 * 128 byte entries (at most 128 of them), that the entry array lies inside the disk after the header, and that the usable range is ordered and inside the disk.
 * @param sector Buffer of `SECTOR_BYTES` bytes with sector 1.
 * @param diskSectors Size of the disk in sectors.
 * @param header Receives the fields on success; unspecified on failure.
 * @return True when the header is valid.
 */
inline bool parseGptHeader(const uint8_t *sector, uint64_t diskSectors,
						   GptHeader &header) {
	if (diskSectors <= GPT_HEADER_SECTOR + 1) return false;
	for (size_t index = 0; index < GPT_SIGNATURE_BYTES; index++)
		if (sector[index] != static_cast<uint8_t>(GPT_SIGNATURE[index])) return false;

	uint32_t headerBytes = detail::readLe32(sector + GPT_HEADER_SIZE_OFFSET);
	if (headerBytes < GPT_MIN_HEADER_BYTES || headerBytes > SECTOR_BYTES) return false;

	uint8_t copy[SECTOR_BYTES];
	for (size_t index = 0; index < headerBytes; index++)
		copy[index] = sector[index];
	for (size_t index = 0; index < GPT_CRC_BYTES; index++)
		copy[GPT_HEADER_CRC_OFFSET + index] = 0;
	if (crc32(copy, headerBytes) != detail::readLe32(sector + GPT_HEADER_CRC_OFFSET))
		return false;

	if (detail::readLe64(sector + GPT_MY_LBA_OFFSET) != GPT_HEADER_SECTOR) return false;

	uint32_t entryBytes = detail::readLe32(sector + GPT_ENTRY_BYTES_OFFSET);
	uint32_t entryCount = detail::readLe32(sector + GPT_ENTRY_COUNT_OFFSET);
	if (entryBytes != GPT_ENTRY_BYTES || entryCount == 0 || entryCount > GPT_MAX_ENTRIES)
		return false;

	header.entriesSector = detail::readLe64(sector + GPT_ENTRIES_LBA_OFFSET);
	header.entryCount = entryCount;
	header.entriesCrc = detail::readLe32(sector + GPT_ENTRIES_CRC_OFFSET);
	header.firstUsable = detail::readLe64(sector + GPT_FIRST_USABLE_OFFSET);
	header.lastUsable = detail::readLe64(sector + GPT_LAST_USABLE_OFFSET);

	if (header.entriesSector <= GPT_HEADER_SECTOR) return false;
	if (!detail::fitsDisk(header.entriesSector, header.entriesSectors(), diskSectors))
		return false;
	return header.firstUsable <= header.lastUsable && header.lastUsable < diskSectors;
}

/**
 * @brief Parses the GPT entry array.
 *
 * Entries with a zero type GUID, an unordered range, or a range outside the usable range or the disk are skipped.
 * @param bytes The entry array, `header.entriesBytes()` bytes read from `header.entriesSector`.
 * @param header Header validated by `parseGptHeader`.
 * @param diskSectors Size of the disk in sectors.
 * @param table Receives the partitions; cleared first.
 * @return False when the CRC32 of the array does not match (the table stays empty); true otherwise.
 */
inline bool parseGptEntries(const uint8_t *bytes, const GptHeader &header,
							uint64_t diskSectors, Table &table) {
	table.scheme = Scheme::NONE;
	table.count = 0;
	if (crc32(bytes, header.entriesBytes()) != header.entriesCrc) return false;

	for (uint32_t slot = 0; slot < header.entryCount; slot++) {
		const uint8_t *entry = bytes + size_t(slot) * GPT_ENTRY_BYTES;
		if (detail::isZero(entry + GPT_TYPE_GUID_OFFSET, GPT_GUID_BYTES)) continue;

		uint64_t first = detail::readLe64(entry + GPT_ENTRY_FIRST_OFFSET);
		uint64_t last = detail::readLe64(entry + GPT_ENTRY_LAST_OFFSET);
		if (first > last || first < header.firstUsable || last > header.lastUsable)
			continue;
		if (!detail::fitsDisk(first, last - first + 1, diskSectors)) continue;
		if (table.count == MAX_PARTITIONS) break;
		table.entries[table.count++] = {first, last - first + 1};
	}
	table.scheme = Scheme::GPT;
	return true;
}

} // namespace cock::abi::partition

#endif // !SHARED_COCK_ABI_PARTITION_HPP
