#include <fat/volume.hpp>

namespace cock::fat {

cock::user::block::Client disk;
uint32_t device = 0;
bool readOnly = true;
cock::abi::fat32::Bpb bpb = {};
uint8_t fatBuffer[cock::abi::fat32::SECTOR_BYTES];
uint32_t fatLoaded = NOTHING_LOADED;
uint8_t directoryBuffer[cock::abi::fat32::SECTOR_BYTES];
uint32_t directoryLoaded = NOTHING_LOADED;
uint8_t dataBuffer[cock::abi::fat32::SECTOR_BYTES];
uint8_t zeros[cock::abi::block::WINDOW_BYTES];
cock::user::Clock clock;
bool clockReady = false;

} // namespace cock::fat
