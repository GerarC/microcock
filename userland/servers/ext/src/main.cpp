#include <cock/user/api.hpp>
#include <ext/grant.hpp>
#include <ext/mount.hpp>
#include <ext/server.hpp>
#include <ext/volume.hpp>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" int main() {
	uint32_t wanted = cock::ext::waitForCapabilities();

	if (!cock::ext::mountVolume(wanted)) cock::user::exit_process(EXIT_FAILED_STATUS);
	printf("ext: device %u, %u blocks of %u bytes, %u inodes\n", cock::ext::device,
		   cock::ext::sb.blocksCount, cock::ext::sb.blockBytes, cock::ext::sb.inodesCount);

	cock::ext::Server::run();
}
