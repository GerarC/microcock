#include <cock/abi/capability.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <rs/boot.hpp>
#include <rs/directory.hpp>
#include <rs/grant.hpp>
#include <stdio.h>

using cock::user::NO_PROCESS;

namespace {

constexpr uintptr_t SINGLE_RESOURCE = 1;

} // namespace

namespace cock::rs {

uint32_t grantInitrd() {
	uint32_t files = cock::user::module_process(cock::abi::fs::RAMFS_SERVER_NAME);
	uint32_t initrd = cock::user::module_index(cock::abi::fs::INITRD_MODULE_NAME);
	if (files == NO_PROCESS || initrd == cock::user::NO_MODULE) return NO_PROCESS;

	if (!cock::user::service_set(files, cock::abi::service::RAMFS_SERVICE))
		printf("rs: cannot label %s\n", cock::abi::fs::RAMFS_SERVER_NAME);
	else
		bindService(cock::abi::service::RAMFS_SERVICE, files);
	printf("rs: initrd is boot module %u\n", initrd);
	cock::user::cap_grant(files, cock::abi::CapabilityType::BOOT_MODULE, initrd, SINGLE_RESOURCE);
	announce(cock::abi::service::RAMFS_SERVICE);
	return files;
}

} // namespace cock::rs
