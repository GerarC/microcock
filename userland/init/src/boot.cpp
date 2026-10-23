#include <cock/abi/capability.hpp>
#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <init/boot.hpp>
#include <init/grant.hpp>
#include <stdio.h>

using cock::user::NO_PROCESS;

namespace {

constexpr uintptr_t SINGLE_RESOURCE = 1;

} // namespace

namespace cock::init {

bool grantInitrd() {
	uint32_t files = cock::user::lookup(cock::abi::fs::RAMFS_SERVER_NAME);
	uint32_t initrd = cock::user::module_index(cock::abi::fs::INITRD_MODULE_NAME);
	if (files == NO_PROCESS || initrd == cock::user::NO_MODULE) return false;

	printf("init: initrd is boot module %u\n", initrd);
	cock::user::cap_grant(files, cock::abi::CapabilityType::BOOT_MODULE, initrd, SINGLE_RESOURCE);
	announce(files, NO_PROCESS);
	return true;
}

} // namespace cock::init
