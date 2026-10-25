#include <rs/files.hpp>

using cock::user::fs::Client;

namespace {

constexpr uint32_t START_OFFSET = 0;
constexpr long MORE_TO_READ = 1;

} // namespace

namespace cock::rs {

bool readInto(Client &client, uint32_t handle, uint8_t *destination, uint32_t size) {
	uint32_t copied = START_OFFSET;
	long step = MORE_TO_READ;
	while (copied < size && step > 0) {
		step = client.read(handle, copied, destination + copied, size - copied);
		if (step > 0) copied += static_cast<uint32_t>(step);
	}
	return copied == size;
}

} // namespace cock::rs
