#include <cock/abi/datetime.hpp>
#include <ext/stamp.hpp>
#include <ext/volume.hpp>

namespace cock::ext {

uint32_t timestamp() {
	cock::abi::datetime::DateTime now = {};
	if (clockReady && clock.now(now)) return cock::abi::datetime::toUnix(now);
	return cock::abi::ext2::DELETION_TIME_MARK;
}

} // namespace cock::ext
