#include <cock/user/api.hpp>
#include <fat/grant.hpp>
#include <fat/mount.hpp>
#include <fat/server.hpp>
#include <fat/volume.hpp>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" void _start() {
	uint32_t wanted = cock::fat::waitForCapabilities();

	if (!cock::fat::mountVolume(wanted)) {
		printf("fat: device %u is not a FAT32 volume\n", wanted);
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("fat: device %u, %u clusters of %u sectors\n", cock::fat::device,
		   cock::fat::bpb.clusterCount, cock::fat::bpb.sectorsPerCluster);

	cock::fat::Server::run();
}
