#include <ext/volume.hpp>

namespace cock::ext {

cock::user::block::Client disk;
uint32_t device = 0;
bool readOnly = true;
cock::user::Clock clock;
bool clockReady = false;
cock::abi::ext2::Superblock sb = {};
uint8_t superblockBuffer[cock::abi::ext2::SUPERBLOCK_BYTES];
uint8_t groupBuffer[SECTOR_BYTES];
uint32_t groupLoaded = NOTHING_LOADED;
uint8_t inodeBuffer[SECTOR_BYTES];
uint32_t inodeLoaded = NOTHING_LOADED;
uint8_t pointerBuffer[SECTOR_BYTES];
uint32_t pointerLoaded = NOTHING_LOADED;
uint8_t bitmapBuffer[SECTOR_BYTES];
uint32_t bitmapLoaded = NOTHING_LOADED;
uint8_t directoryBuffer[cock::abi::ext2::MAX_BLOCK_BYTES];
uint32_t directoryLoaded = NOTHING_LOADED;
uint8_t dataBuffer[SECTOR_BYTES];
uint8_t zeros[cock::abi::ext2::MAX_BLOCK_BYTES];

bool loadSector(uint32_t sector, uint8_t *buffer, uint32_t &loaded) {
	if (sector == loaded) return true;
	loaded = NOTHING_LOADED;
	if (!disk.read(device, sector, SINGLE_SECTOR, buffer)) return false;
	loaded = sector;
	return true;
}

bool storeSector(uint32_t sector, const uint8_t *buffer, uint32_t &loaded) {
	if (disk.write(device, sector, SINGLE_SECTOR, buffer)) return true;
	loaded = NOTHING_LOADED;
	return false;
}

} // namespace cock::ext
