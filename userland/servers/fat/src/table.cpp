#include <fat/fsinfo.hpp>
#include <fat/table.hpp>
#include <fat/volume.hpp>

using cock::abi::fs::Status;

namespace cock::fat {

uint32_t fatSector(uint32_t cluster) {
	return bpb.reservedSectors + cluster / cock::abi::fat32::FAT_ENTRIES_PER_SECTOR;
}

bool loadFat(uint32_t sector) {
	if (sector == fatLoaded) return true;
	fatLoaded = NOTHING_LOADED;
	if (!disk.read(device, sector, SINGLE_SECTOR, fatBuffer)) return false;
	fatLoaded = sector;
	return true;
}

bool storeFat() {
	for (uint32_t copy = 0; copy < bpb.fatCount; copy++) {
		if (!disk.write(device, fatLoaded + copy * bpb.fatSectors, SINGLE_SECTOR, fatBuffer)) {
			fatLoaded = NOTHING_LOADED;
			return false;
		}
	}
	return true;
}

bool setLink(uint32_t cluster, uint32_t value) {
	if (!loadFat(fatSector(cluster))) return false;
	cock::abi::fat32::setFatValue(
		fatBuffer, cluster % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR, value);
	return storeFat();
}

Link nextCluster(uint32_t cluster, uint32_t &next) {
	if (!loadFat(fatSector(cluster))) return Link::BAD;

	uint32_t value = cock::abi::fat32::fatValue(
		fatBuffer, cluster % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR);
	if (cock::abi::fat32::isEndOfChain(value)) return Link::END;
	if (!bpb.isDataCluster(value)) return Link::BAD;
	next = value;
	return Link::NEXT;
}

Status allocateCluster(uint32_t previous, uint32_t &cluster) {
	uint32_t start =
		bpb.isDataCluster(nextHint) ? nextHint : cock::abi::fat32::FIRST_DATA_CLUSTER;
	for (uint32_t step = 0; step < bpb.clusterCount; step++) {
		uint32_t candidate = cock::abi::fat32::FIRST_DATA_CLUSTER +
							 (start - cock::abi::fat32::FIRST_DATA_CLUSTER + step) %
								 bpb.clusterCount;
		if (!loadFat(fatSector(candidate))) return Status::IO_ERROR;
		uint32_t index = candidate % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR;
		if (cock::abi::fat32::fatValue(fatBuffer, index) != cock::abi::fat32::FAT_FREE) continue;

		cock::abi::fat32::setFatValue(fatBuffer, index, cock::abi::fat32::FAT_END_OF_CHAIN);
		bool linked = false;
		if (previous != NO_CLUSTER && fatSector(previous) == fatLoaded) {
			cock::abi::fat32::setFatValue(
				fatBuffer, previous % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR, candidate);
			linked = true;
		}
		if (!storeFat()) return Status::IO_ERROR;
		if (previous != NO_CLUSTER && !linked && !setLink(previous, candidate))
			return Status::IO_ERROR;

		nextHint = candidate + 1;
		if (freeKnown && freeCount > 0) freeCount--;
		fsInfoDirty = true;
		cluster = candidate;
		return Status::OK;
	}
	return Status::NO_SPACE;
}

bool freeChain(uint32_t first) {
	uint32_t cluster = first;
	uint32_t steps = 0;
	while (bpb.isDataCluster(cluster)) {
		uint32_t next = NO_CLUSTER;
		Link link = nextCluster(cluster, next);
		if (link == Link::BAD) return false;
		if (!setLink(cluster, cock::abi::fat32::FAT_FREE)) return false;

		if (freeKnown) freeCount++;
		if (cluster < nextHint) nextHint = cluster;
		fsInfoDirty = true;
		if (link == Link::END) return true;
		cluster = next;
		if (++steps > bpb.clusterCount) return false;
	}
	return false;
}

bool zeroCluster(uint32_t cluster) {
	directoryLoaded = NOTHING_LOADED;
	uint32_t sector = bpb.clusterSector(cluster);
	uint32_t left = bpb.sectorsPerCluster;
	while (left > 0) {
		uint32_t run = left < cock::abi::block::MAX_SECTORS_PER_WRITE
						   ? left
						   : cock::abi::block::MAX_SECTORS_PER_WRITE;
		if (!disk.write(device, sector, run, zeros)) return false;
		sector += run;
		left -= run;
	}
	return true;
}

} // namespace cock::fat
