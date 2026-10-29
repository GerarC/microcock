#include <fat/fsinfo.hpp>
#include <fat/volume.hpp>

namespace {

constexpr uint32_t FIRST_FSINFO_SECTOR = 1;

} // namespace

namespace cock::fat {

uint8_t fsInfoBuffer[cock::abi::fat32::SECTOR_BYTES];
bool fsInfoValid = false;
bool fsInfoDirty = false;
bool freeKnown = false;
uint32_t freeCount = 0;
uint32_t nextHint = cock::abi::fat32::FIRST_DATA_CLUSTER;

void loadFsInfo() {
	fsInfoValid = false;
	freeKnown = false;
	nextHint = cock::abi::fat32::FIRST_DATA_CLUSTER;
	if (bpb.fsInfoSector < FIRST_FSINFO_SECTOR || bpb.fsInfoSector >= bpb.reservedSectors) return;
	if (!disk.read(device, bpb.fsInfoSector, SINGLE_SECTOR, fsInfoBuffer) ||
		!cock::abi::fat32::validFsInfo(fsInfoBuffer))
		return;

	fsInfoValid = true;
	uint32_t known = cock::abi::fat32::fsInfoFree(fsInfoBuffer);
	freeKnown = known != cock::abi::fat32::FSINFO_UNKNOWN && known <= bpb.clusterCount;
	freeCount = freeKnown ? known : 0;
	uint32_t hint = cock::abi::fat32::fsInfoNext(fsInfoBuffer);
	if (bpb.isDataCluster(hint)) nextHint = hint;
}

void flushFsInfo() {
	if (!fsInfoValid || !fsInfoDirty) return;
	fsInfoDirty = false;
	cock::abi::fat32::storeFsInfo(
		fsInfoBuffer, freeKnown ? freeCount : cock::abi::fat32::FSINFO_UNKNOWN, nextHint);
	disk.write(device, bpb.fsInfoSector, SINGLE_SECTOR, fsInfoBuffer);
}

} // namespace cock::fat
