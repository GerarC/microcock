#include <ata/devices.hpp>
#include <cock/abi/partition.hpp>
#include <stdio.h>

using cock::abi::block::DeviceKind;
using cock::abi::partition::GptHeader;
using cock::abi::partition::MbrResult;
using cock::abi::partition::Table;

namespace cock::ata {

namespace {

constexpr uint32_t DISK_FIRST_SECTOR = 0;
constexpr uint32_t MBR_SECTOR = 0;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr uint32_t GPT_HEADER_SECTOR = cock::abi::partition::GPT_HEADER_SECTOR;
constexpr uint32_t NO_DEVICES = 0;

uint8_t sectorBuffer[cock::abi::block::SECTOR_BYTES];
uint8_t entriesBuffer[cock::abi::partition::GPT_ENTRIES_MAX_BYTES];

void addDevice(uint32_t first, uint32_t count, DeviceKind kind) {
	devices[deviceCount++] = {first, count, kind};
}

void addPartitions(const Table &table, DeviceKind kind) {
	for (size_t slot = 0; slot < table.count; slot++)
		addDevice(static_cast<uint32_t>(table.entries[slot].firstSector),
				  static_cast<uint32_t>(table.entries[slot].sectorCount), kind);
}

bool readGptEntries(const GptHeader &header, uint32_t diskSectors, Table &table) {
	size_t sectors = header.entriesSectors();
	for (size_t index = 0; index < sectors; index++) {
		if (!drive.read(static_cast<uint32_t>(header.entriesSector + index), SINGLE_SECTOR,
						entriesBuffer + index * cock::abi::block::SECTOR_BYTES))
			return false;
	}
	return cock::abi::partition::parseGptEntries(entriesBuffer, header, diskSectors, table);
}

} // namespace

Drive drive;
Device devices[cock::abi::block::MAX_DEVICES];
uint32_t deviceCount = NO_DEVICES;

void scanPartitions() {
	addDevice(DISK_FIRST_SECTOR, drive.sectors(), DeviceKind::DISK);
	uint32_t diskSectors = drive.sectors();

	Table table = {};
	if (!drive.read(MBR_SECTOR, SINGLE_SECTOR, sectorBuffer)) return;

	MbrResult result = cock::abi::partition::parseMbr(sectorBuffer, diskSectors, table);
	if (result == MbrResult::PRIMARY) return addPartitions(table, DeviceKind::MBR_PARTITION);
	if (result != MbrResult::PROTECTIVE) return;

	GptHeader header = {};
	if (!drive.read(GPT_HEADER_SECTOR, SINGLE_SECTOR, sectorBuffer) ||
		!cock::abi::partition::parseGptHeader(sectorBuffer, diskSectors, header) ||
		!readGptEntries(header, diskSectors, table)) {
		printf("ata: GPT rejected\n");
		return;
	}
	addPartitions(table, DeviceKind::GPT_PARTITION);
}

} // namespace cock::ata
