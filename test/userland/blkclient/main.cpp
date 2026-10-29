#include <cock/abi/block.hpp>
#include <cock/user/api.hpp>
#include <cock/user/block.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::user::block::DeviceInfo;

namespace {

constexpr uint32_t FIRST_PARTITION = 1;
constexpr uint32_t FIRST_SECTOR = 0;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr size_t TEXT_BYTES = 32;
constexpr uintptr_t EXIT_SERVED = 0;
constexpr uintptr_t EXIT_REFUSED = 1;

uint8_t sector[cock::abi::block::SECTOR_BYTES];

}

extern "C" int main() {
	cock::user::block::Client client;
	bool served = client.connect() && client.deviceCount() > FIRST_PARTITION;

	for (uint32_t device = 0; served && device < client.deviceCount(); device++) {
		DeviceInfo info = {};
		served = client.info(device, info);
		if (served)
			printf("blkclient: device %u: kind %u first %u sectors %u\n", device,
				   static_cast<unsigned>(info.kind), info.firstSector,
				   info.sectorCount);
	}

	served = served && client.read(FIRST_PARTITION, FIRST_SECTOR, SINGLE_SECTOR, sector);
	if (served) {
		sector[TEXT_BYTES - 1] = '\0';
		printf("blkclient: sector says %s\n", reinterpret_cast<const char *>(sector));
	}

	puts(served ? "blkclient: ok" : "blkclient: refused");
	cock::user::exit_process(served ? EXIT_SERVED : EXIT_REFUSED);
}
