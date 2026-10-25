#include <cock/abi/fs.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <init/directory.hpp>
#include <init/endpoints.hpp>
#include <init/manifest.hpp>
#include <init/service.hpp>
#include <stdio.h>
#include <string.h>

using cock::abi::manifest::Entry;
using cock::abi::service::NAMES_SERVICE;
using cock::abi::service::NO_SERVICE;
using cock::abi::service::PROGRAM_SERVICE_BASE;
using cock::abi::service::RAMFS_SERVICE;
using cock::abi::service::SpawnRights;

namespace cock::init {

uint32_t labelOf(const Entry &entry) {
	return PROGRAM_SERVICE_BASE + static_cast<uint32_t>(&entry - manifest.entries);
}

uint32_t labelOfName(const char *name) {
	if (strcmp(name, cock::abi::fs::RAMFS_SERVER_NAME) == 0) return RAMFS_SERVICE;
	const Entry *entry = manifest.find(name);
	return entry == nullptr ? NO_SERVICE : labelOf(*entry);
}

SpawnRights rightsOf(const Entry &entry) {
	SpawnRights rights = {};
	rights.label = labelOf(entry);
	rights.initialEndpoint = endpointOf(NAMES_SERVICE);
	return rights;
}

bool labelBootModule(const Entry &entry, uint32_t pid) {
	if (!cock::user::service_set(pid, labelOf(entry))) {
		printf("init: cannot label %s\n", entry.name);
		return false;
	}
	return bindService(labelOf(entry), pid);
}

} // namespace cock::init
