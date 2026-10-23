#include <fat/state.hpp>
#include <fat/table.hpp>
#include <fat/volume.hpp>

using cock::abi::fs::Status;

namespace {

bool setCleanBit(bool clean) {
	using namespace cock::fat;
	if (!loadFat(fatSector(cock::abi::fat32::FAT_STATE_ENTRY))) return false;
	uint8_t *entry = fatBuffer + size_t(cock::abi::fat32::FAT_STATE_ENTRY) *
									 cock::abi::fat32::FAT_ENTRY_BYTES;
	uint32_t value = cock::abi::fat32::detail::readLe32(entry);
	value = clean ? (value | cock::abi::fat32::FAT_CLEAN_SHUTDOWN_BIT)
				  : (value & ~cock::abi::fat32::FAT_CLEAN_SHUTDOWN_BIT);
	cock::abi::fat32::detail::writeLe32(entry, value);
	return storeFat();
}

} // namespace

namespace cock::fat {

bool isCleanVolume() {
	if (!loadFat(fatSector(cock::abi::fat32::FAT_STATE_ENTRY))) return false;
	const uint8_t *entry = fatBuffer + size_t(cock::abi::fat32::FAT_STATE_ENTRY) *
										   cock::abi::fat32::FAT_ENTRY_BYTES;
	return (cock::abi::fat32::detail::readLe32(entry) &
			cock::abi::fat32::FAT_CLEAN_SHUTDOWN_BIT) != 0;
}

bool beginUpdate() { return setCleanBit(false); }

Status endUpdate(Status status) { return setCleanBit(true) ? status : Status::IO_ERROR; }

} // namespace cock::fat
