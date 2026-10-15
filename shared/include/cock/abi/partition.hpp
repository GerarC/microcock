#ifndef SHARED_COCK_ABI_PARTITION_HPP
#define SHARED_COCK_ABI_PARTITION_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::partition {

constexpr size_t SECTOR_BYTES = 512;
constexpr size_t MAX_PARTITIONS = 16;

constexpr size_t MBR_SIGNATURE_OFFSET = 510;
constexpr uint8_t MBR_SIGNATURE_FIRST = 0x55;
constexpr uint8_t MBR_SIGNATURE_SECOND = 0xAA;
constexpr size_t MBR_SIGNATURE_SECOND_STEP = 1;
constexpr size_t MBR_TABLE_OFFSET = 446;
constexpr size_t MBR_ENTRY_COUNT = 4;
constexpr size_t MBR_ENTRY_BYTES = 16;
constexpr size_t MBR_TYPE_OFFSET = 4;
constexpr size_t MBR_FIRST_SECTOR_OFFSET = 8;
constexpr size_t MBR_SECTOR_COUNT_OFFSET = 12;
constexpr uint8_t MBR_TYPE_EMPTY = 0x00;
constexpr uint8_t MBR_TYPE_PROTECTIVE = 0xEE;
constexpr uint8_t MBR_TYPE_EXTENDED_CHS = 0x05;
constexpr uint8_t MBR_TYPE_EXTENDED_LBA = 0x0F;
constexpr uint8_t MBR_TYPE_EXTENDED_LINUX = 0x85;

constexpr size_t GPT_HEADER_SECTOR = 1;
constexpr size_t GPT_MIN_HEADER_BYTES = 92;
constexpr size_t GPT_SIGNATURE_BYTES = 8;
constexpr char GPT_SIGNATURE[GPT_SIGNATURE_BYTES + 1] = "EFI PART";
constexpr size_t GPT_HEADER_SIZE_OFFSET = 12;
constexpr size_t GPT_HEADER_CRC_OFFSET = 16;
constexpr size_t GPT_CRC_BYTES = 4;
constexpr size_t GPT_MY_LBA_OFFSET = 24;
constexpr size_t GPT_FIRST_USABLE_OFFSET = 40;
constexpr size_t GPT_LAST_USABLE_OFFSET = 48;
constexpr size_t GPT_ENTRIES_LBA_OFFSET = 72;
constexpr size_t GPT_ENTRY_COUNT_OFFSET = 80;
constexpr size_t GPT_ENTRY_BYTES_OFFSET = 84;
constexpr size_t GPT_ENTRIES_CRC_OFFSET = 88;
constexpr size_t GPT_MAX_ENTRIES = 128;
constexpr size_t GPT_ENTRY_BYTES = 128;
constexpr size_t GPT_ENTRIES_MAX_BYTES = GPT_MAX_ENTRIES * GPT_ENTRY_BYTES;
constexpr size_t GPT_TYPE_GUID_OFFSET = 0;
constexpr size_t GPT_GUID_BYTES = 16;
constexpr size_t GPT_ENTRY_FIRST_OFFSET = 32;
constexpr size_t GPT_ENTRY_LAST_OFFSET = 40;

constexpr uint32_t CRC32_POLYNOMIAL = 0xEDB88320;
constexpr uint32_t CRC32_INITIAL = 0xFFFFFFFF;
constexpr uint32_t CRC32_LOW_BIT = 1;
constexpr unsigned CRC32_SHIFT = 1;
constexpr unsigned BITS_PER_BYTE = 8;
constexpr unsigned BYTE_MASK = 0xFF;
constexpr unsigned SHIFT_16 = 16;
constexpr unsigned SHIFT_24 = 24;
constexpr unsigned SHIFT_32 = 32;
constexpr unsigned LAST_BYTE_INDEX = 3;

enum class Scheme : uint32_t {
	NONE = 0,
	MBR = 1,
	GPT = 2,
};

enum class MbrResult : uint32_t {
	INVALID = 0,
	PRIMARY = 1,
	PROTECTIVE = 2,
};

struct Entry {
	uint64_t firstSector;
	uint64_t sectorCount;
};

struct Table {
	Scheme scheme;
	Entry entries[MAX_PARTITIONS];
	size_t count;
};

struct GptHeader {
	uint64_t entriesSector;
	uint32_t entryCount;
	uint32_t entriesCrc;
	uint64_t firstUsable;
	uint64_t lastUsable;

	size_t entriesBytes() const { return size_t(entryCount) * GPT_ENTRY_BYTES; }

	size_t entriesSectors() const {
		return (entriesBytes() + SECTOR_BYTES - 1) / SECTOR_BYTES;
	}
};

namespace detail {

inline uint32_t readLe32(const uint8_t *bytes) {
	return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << BITS_PER_BYTE) |
		   (uint32_t(bytes[2]) << SHIFT_16) | (uint32_t(bytes[LAST_BYTE_INDEX]) << SHIFT_24);
}

inline uint64_t readLe64(const uint8_t *bytes) {
	return uint64_t(readLe32(bytes)) |
		   (uint64_t(readLe32(bytes + sizeof(uint32_t))) << SHIFT_32);
}

inline bool isExtended(uint8_t type) {
	return type == MBR_TYPE_EXTENDED_CHS || type == MBR_TYPE_EXTENDED_LBA ||
		   type == MBR_TYPE_EXTENDED_LINUX;
}

inline bool fitsDisk(uint64_t first, uint64_t count, uint64_t diskSectors) {
	return count != 0 && first < diskSectors && count <= diskSectors - first;
}

inline bool isZero(const uint8_t *bytes, size_t length) {
	for (size_t index = 0; index < length; index++)
		if (bytes[index] != 0) return false;
	return true;
}

} // namespace detail

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
