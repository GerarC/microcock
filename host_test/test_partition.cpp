#include <cock/abi/partition.hpp>
#include <host_test/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using namespace cock::abi::partition;

namespace {

constexpr uint64_t DISK_SECTORS = 16384;
constexpr uint8_t LINUX_TYPE = 0x83;
constexpr uint8_t FAT_TYPE = 0x0B;
constexpr uint32_t FIRST_START = 2048;
constexpr uint32_t FIRST_COUNT = 2048;
constexpr uint32_t SECOND_START = 4096;
constexpr uint32_t SECOND_COUNT = 4096;
constexpr size_t FIRST_SLOT = 0;
constexpr size_t SECOND_SLOT = 1;
constexpr size_t THIRD_SLOT = 2;
constexpr size_t NO_PARTITIONS = 0;
constexpr size_t TWO_PARTITIONS = 2;
constexpr size_t ONE_PARTITION = 1;
constexpr uint32_t CRC_CHECK_VALUE = 0xCBF43926;
constexpr char CRC_CHECK_TEXT[] = "123456789";
constexpr size_t CRC_CHECK_BYTES = 9;
constexpr uint32_t GPT_ENTRIES_SECTOR = 2;
constexpr uint32_t GPT_ENTRY_COUNT = 128;
constexpr uint32_t GPT_HEADER_BYTES = 92;
constexpr uint64_t GPT_FIRST_USABLE = 34;
constexpr uint64_t GPT_LAST_USABLE = DISK_SECTORS - 34;
constexpr uint8_t GUID_FILL = 0x11;
constexpr uint32_t BEYOND_DISK_COUNT = 1000000;
constexpr uint8_t HEADER_CORRUPTION = 0xFF;
constexpr size_t CORRUPTED_BYTE = 60;
constexpr size_t PADDING_BYTE = 200;
constexpr uint32_t BYTES_IN_WORD = 4;
constexpr uint32_t BITS_IN_BYTE = 8;

void putLe32(uint8_t *at, uint32_t value) {
	for (uint32_t index = 0; index < BYTES_IN_WORD; index++)
		at[index] = static_cast<uint8_t>(value >> (index * BITS_IN_BYTE));
}

void putLe64(uint8_t *at, uint64_t value) {
	putLe32(at, static_cast<uint32_t>(value));
	putLe32(at + BYTES_IN_WORD, static_cast<uint32_t>(value >> SHIFT_32));
}

void putMbrEntry(uint8_t *sector, size_t slot, uint8_t type, uint32_t first,
				 uint32_t count) {
	uint8_t *entry = sector + MBR_TABLE_OFFSET + slot * MBR_ENTRY_BYTES;
	entry[MBR_TYPE_OFFSET] = type;
	putLe32(entry + MBR_FIRST_SECTOR_OFFSET, first);
	putLe32(entry + MBR_SECTOR_COUNT_OFFSET, count);
}

void signMbr(uint8_t *sector) {
	sector[MBR_SIGNATURE_OFFSET] = MBR_SIGNATURE_FIRST;
	sector[MBR_SIGNATURE_OFFSET + MBR_SIGNATURE_SECOND_STEP] = MBR_SIGNATURE_SECOND;
}

void checkMbr() {
	uint8_t sector[SECTOR_BYTES] = {};
	Table table = {};
	CHECK_TRUE(parseMbr(sector, DISK_SECTORS, table) == MbrResult::INVALID);

	signMbr(sector);
	putMbrEntry(sector, FIRST_SLOT, LINUX_TYPE, FIRST_START, FIRST_COUNT);
	putMbrEntry(sector, SECOND_SLOT, FAT_TYPE, SECOND_START, SECOND_COUNT);
	CHECK_TRUE(parseMbr(sector, DISK_SECTORS, table) == MbrResult::PRIMARY);
	CHECK_TRUE(table.scheme == Scheme::MBR);
	CHECK_EQ(TWO_PARTITIONS, table.count);
	CHECK_EQ(uint64_t(FIRST_START), table.entries[FIRST_SLOT].firstSector);
	CHECK_EQ(uint64_t(FIRST_COUNT), table.entries[FIRST_SLOT].sectorCount);
	CHECK_EQ(uint64_t(SECOND_START), table.entries[SECOND_SLOT].firstSector);
	CHECK_EQ(uint64_t(SECOND_COUNT), table.entries[SECOND_SLOT].sectorCount);
}

void checkMbrRejections() {
	uint8_t sector[SECTOR_BYTES] = {};
	Table table = {};
	signMbr(sector);

	putMbrEntry(sector, FIRST_SLOT, LINUX_TYPE, FIRST_START, BEYOND_DISK_COUNT);
	putMbrEntry(sector, SECOND_SLOT, MBR_TYPE_EXTENDED_LBA, SECOND_START, SECOND_COUNT);
	putMbrEntry(sector, THIRD_SLOT, LINUX_TYPE, SECOND_START, 0);
	CHECK_TRUE(parseMbr(sector, DISK_SECTORS, table) == MbrResult::PRIMARY);
	CHECK_EQ(NO_PARTITIONS, table.count);

	putMbrEntry(sector, FIRST_SLOT, LINUX_TYPE, static_cast<uint32_t>(DISK_SECTORS), 1);
	CHECK_TRUE(parseMbr(sector, DISK_SECTORS, table) == MbrResult::PRIMARY);
	CHECK_EQ(NO_PARTITIONS, table.count);

	putMbrEntry(sector, FIRST_SLOT, MBR_TYPE_PROTECTIVE, 1,
				static_cast<uint32_t>(DISK_SECTORS - 1));
	CHECK_TRUE(parseMbr(sector, DISK_SECTORS, table) == MbrResult::PROTECTIVE);
	CHECK_EQ(NO_PARTITIONS, table.count);
}

void checkCrc() {
	CHECK_EQ(CRC_CHECK_VALUE,
			 crc32(reinterpret_cast<const uint8_t *>(CRC_CHECK_TEXT), CRC_CHECK_BYTES));
}

struct GptImage {
	uint8_t header[SECTOR_BYTES];
	uint8_t entries[GPT_ENTRIES_MAX_BYTES];
};

void putGptEntry(GptImage &image, size_t slot, uint64_t first, uint64_t last) {
	uint8_t *entry = image.entries + slot * GPT_ENTRY_BYTES;
	memset(entry + GPT_TYPE_GUID_OFFSET, GUID_FILL, GPT_GUID_BYTES);
	putLe64(entry + GPT_ENTRY_FIRST_OFFSET, first);
	putLe64(entry + GPT_ENTRY_LAST_OFFSET, last);
}

void sealGpt(GptImage &image) {
	memcpy(image.header, GPT_SIGNATURE, GPT_SIGNATURE_BYTES);
	putLe32(image.header + GPT_HEADER_SIZE_OFFSET, GPT_HEADER_BYTES);
	putLe64(image.header + GPT_MY_LBA_OFFSET, GPT_HEADER_SECTOR);
	putLe64(image.header + GPT_FIRST_USABLE_OFFSET, GPT_FIRST_USABLE);
	putLe64(image.header + GPT_LAST_USABLE_OFFSET, GPT_LAST_USABLE);
	putLe64(image.header + GPT_ENTRIES_LBA_OFFSET, GPT_ENTRIES_SECTOR);
	putLe32(image.header + GPT_ENTRY_COUNT_OFFSET, GPT_ENTRY_COUNT);
	putLe32(image.header + GPT_ENTRY_BYTES_OFFSET, GPT_ENTRY_BYTES);
	putLe32(image.header + GPT_ENTRIES_CRC_OFFSET,
			crc32(image.entries, GPT_ENTRIES_MAX_BYTES));
	putLe32(image.header + GPT_HEADER_CRC_OFFSET, 0);
	putLe32(image.header + GPT_HEADER_CRC_OFFSET, crc32(image.header, GPT_HEADER_BYTES));
}

void checkGpt() {
	GptImage image = {};
	putGptEntry(image, FIRST_SLOT, FIRST_START, FIRST_START + FIRST_COUNT - 1);
	putGptEntry(image, SECOND_SLOT, SECOND_START, SECOND_START + SECOND_COUNT - 1);
	sealGpt(image);

	GptHeader header = {};
	CHECK_TRUE(parseGptHeader(image.header, DISK_SECTORS, header));
	CHECK_EQ(uint64_t(GPT_ENTRIES_SECTOR), header.entriesSector);
	CHECK_EQ(GPT_ENTRY_COUNT, header.entryCount);

	Table table = {};
	CHECK_TRUE(parseGptEntries(image.entries, header, DISK_SECTORS, table));
	CHECK_TRUE(table.scheme == Scheme::GPT);
	CHECK_EQ(TWO_PARTITIONS, table.count);
	CHECK_EQ(uint64_t(FIRST_START), table.entries[FIRST_SLOT].firstSector);
	CHECK_EQ(uint64_t(FIRST_COUNT), table.entries[FIRST_SLOT].sectorCount);
	CHECK_EQ(uint64_t(SECOND_START), table.entries[SECOND_SLOT].firstSector);
	CHECK_EQ(uint64_t(SECOND_COUNT), table.entries[SECOND_SLOT].sectorCount);
}

void checkGptRejections() {
	GptImage image = {};
	putGptEntry(image, FIRST_SLOT, FIRST_START, FIRST_START + FIRST_COUNT - 1);
	putGptEntry(image, SECOND_SLOT, GPT_FIRST_USABLE - 1, FIRST_START);
	putGptEntry(image, THIRD_SLOT, SECOND_START, DISK_SECTORS);
	sealGpt(image);

	GptHeader header = {};
	CHECK_TRUE(parseGptHeader(image.header, DISK_SECTORS, header));
	Table table = {};
	CHECK_TRUE(parseGptEntries(image.entries, header, DISK_SECTORS, table));
	CHECK_EQ(ONE_PARTITION, table.count);

	image.entries[PADDING_BYTE] ^= HEADER_CORRUPTION;
	CHECK_FALSE(parseGptEntries(image.entries, header, DISK_SECTORS, table));
	image.entries[PADDING_BYTE] ^= HEADER_CORRUPTION;

	GptImage corrupted = image;
	corrupted.header[CORRUPTED_BYTE] ^= HEADER_CORRUPTION;
	CHECK_FALSE(parseGptHeader(corrupted.header, DISK_SECTORS, header));

	GptImage unsignedImage = image;
	unsignedImage.header[0] = 0;
	CHECK_FALSE(parseGptHeader(unsignedImage.header, DISK_SECTORS, header));

	CHECK_FALSE(parseGptHeader(image.header, GPT_ENTRIES_SECTOR, header));

	GptImage misplaced = image;
	putLe64(misplaced.header + GPT_ENTRIES_LBA_OFFSET, DISK_SECTORS);
	putLe32(misplaced.header + GPT_HEADER_CRC_OFFSET, 0);
	putLe32(misplaced.header + GPT_HEADER_CRC_OFFSET,
			crc32(misplaced.header, GPT_HEADER_BYTES));
	CHECK_FALSE(parseGptHeader(misplaced.header, DISK_SECTORS, header));
}

} // namespace

void testPartition() {
	checkCrc();
	checkMbr();
	checkMbrRejections();
	checkGpt();
	checkGptRejections();
}

} // namespace host_test
