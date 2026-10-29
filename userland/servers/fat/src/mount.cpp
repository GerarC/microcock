#include <fat/fsinfo.hpp>
#include <fat/mount.hpp>
#include <fat/server.hpp>
#include <fat/state.hpp>
#include <fat/volume.hpp>

using cock::abi::fat32::Bpb;

namespace {

constexpr uint32_t SECTOR_ZERO = 0;

} // namespace

namespace cock::fat {

bool mountVolume(uint32_t candidate) {
	disk.onForeign = Server::defer;
	clock.onForeign = Server::defer;
	if (!disk.connect() || candidate >= disk.deviceCount()) return false;

	cock::user::block::DeviceInfo info = {};
	Bpb found = {};
	if (!disk.info(candidate, info) || !disk.read(candidate, SECTOR_ZERO, SINGLE_SECTOR, dataBuffer))
		return false;
	if (cock::abi::fat32::parseBpb(dataBuffer, info.sectorCount, found) !=
		cock::abi::fat32::BpbError::NONE)
		return false;

	bpb = found;
	device = candidate;
	readOnly = info.kind == cock::abi::block::DeviceKind::DISK || !isCleanVolume();
	clockReady = clock.init();
	loadFsInfo();
	return true;
}

} // namespace cock::fat
