#include <cock/abi/datetime.hpp>
#include <fat/stamp.hpp>
#include <fat/volume.hpp>

namespace cock::fat {

void currentStamp(uint16_t &date, uint16_t &time) {
	cock::abi::datetime::DateTime now = {};
	if (clockReady && clock.now(now)) {
		date = cock::abi::datetime::toFatDate(now);
		time = cock::abi::datetime::toFatTime(now);
		return;
	}
	date = cock::abi::fat32::FAT_EPOCH_DATE;
	time = cock::abi::fat32::FAT_EPOCH_TIME;
}

} // namespace cock::fat
