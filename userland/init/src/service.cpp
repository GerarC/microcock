#include <cock/abi/capability.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <init/manifest.hpp>
#include <init/service.hpp>
#include <stdio.h>
#include <string.h>

using cock::abi::CapabilityType;
using cock::abi::manifest::Entry;
using cock::abi::service::NO_SERVICE;
using cock::abi::service::PROGRAM_SERVICE_BASE;
using cock::abi::service::RAMFS_SERVICE;

namespace {

constexpr uintptr_t SINGLE_LABEL = 1;

} // namespace

namespace cock::init {

uint32_t labelOf(const Entry &entry) {
	return PROGRAM_SERVICE_BASE + static_cast<uint32_t>(&entry - manifest.entries);
}

uint32_t labelOfName(const char *name) {
	if (strcmp(name, cock::abi::fs::RAMFS_SERVER_NAME) == 0) return RAMFS_SERVICE;
	const Entry *entry = manifest.find(name);
	return entry == nullptr ? NO_SERVICE : labelOf(*entry);
}

void connectProgram(const Entry &entry, uint32_t pid) {
	if (!cock::user::service_set(pid, labelOf(entry)) && cock::user::process_alive(pid))
		printf("init: cannot label %s\n", entry.name);

	for (size_t index = 0; index < entry.ipcCount; index++) {
		uint32_t label = labelOfName(entry.ipc[index]);
		bool unknown = label == NO_SERVICE;
		if (unknown ||
			(!cock::user::cap_grant(pid, CapabilityType::IPC, label, SINGLE_LABEL) &&
			 cock::user::process_alive(pid)))
			printf("init: %s cannot talk to %s\n", entry.name, entry.ipc[index]);
	}
}

} // namespace cock::init
