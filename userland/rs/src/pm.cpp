#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/pm.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <rs/directory.hpp>
#include <rs/endpoints.hpp>
#include <rs/grant.hpp>
#include <rs/manifest.hpp>
#include <rs/pm.hpp>
#include <rs/service.hpp>
#include <rs/spawn.hpp>
#include <stdio.h>
#include <string.h>

using cock::abi::CapabilityType;
using cock::abi::manifest::Entry;
using cock::abi::service::NAMES_SERVICE;
using cock::abi::service::ROOT_SERVICE;
using cock::user::NO_PROCESS;

namespace {

constexpr char NAMES_MODULE[] = "names";
constexpr uint32_t NO_FLAGS = 0;

const Entry *entry = nullptr;
uint32_t process = NO_PROCESS;
cock::user::pm::Client client;

bool equipPm(const Entry &manifestEntry) {
	cock::rs::grantCapabilities(manifestEntry, process);
	bool granted =
		cock::user::cap_grant(process, CapabilityType::IPC, cock::abi::service::PROGRAM_SERVICE_BASE,
							  cock::abi::pm::PROGRAM_LABEL_COUNT, true) &&
		cock::user::cap_grant(process, CapabilityType::ENDPOINT, cock::rs::endpointOf(NAMES_SERVICE),
							  cock::abi::endpoint::SINGLE_ENDPOINT, true, true);
	if (!granted) {
		printf("rs: cannot equip pm\n");
		return false;
	}

	uint32_t ownEndpoint = cock::rs::bindEndpoint(ROOT_SERVICE, cock::user::self());
	if (ownEndpoint == cock::abi::endpoint::NO_ENDPOINT) {
		printf("rs: cannot bind the endpoint of rs\n");
		return false;
	}
	uint32_t back = cock::user::cap_grant_node(process, CapabilityType::ENDPOINT, ownEndpoint,
											   cock::abi::endpoint::SINGLE_ENDPOINT, false, false, process);
	uint32_t toPm = cock::rs::handleOf(cock::rs::labelOf(manifestEntry));
	if (back == cock::abi::NO_CAPABILITY_NODE || toPm == cock::abi::NO_CAPABILITY_NODE) {
		printf("rs: no handle between rs and pm\n");
		return false;
	}

	client.connect(toPm);
	cock::rs::announce(cock::rs::labelOf(manifestEntry), back, cock::rs::endpointOf(NAMES_SERVICE));
	return true;
}

bool recordParent(uint32_t pid, uint32_t parent) {
	uint32_t pidInTable = cock::abi::pm::NO_PID;
	return client.registerProcess(pid, parent, cock::abi::pm::NO_PID, pidInTable);
}

} // namespace

namespace cock::rs {

bool startPm(cock::user::fs::Client &files, uint32_t ramfs) {
	entry = manifest.find(PM_PROGRAM_NAME);
	if (entry == nullptr) {
		printf("rs: no pm in the manifest\n");
		return false;
	}

	process = spawnDirect(files, *entry);
	if (process == NO_PROCESS) return false;
	bindService(labelOf(*entry), process);
	if (!equipPm(*entry)) return false;

	uint32_t self = cock::user::self();
	uint32_t names = cock::user::module_process(NAMES_MODULE);
	bool recorded = recordParent(self, cock::abi::pm::NO_PARENT) && recordParent(process, self) &&
					recordParent(names, self) && recordParent(ramfs, self);
	if (!recorded) {
		printf("rs: pm did not record the first processes\n");
		return false;
	}

	for (size_t index = 0; index < manifest.count; index++) {
		if (!manifest.entries[index].startedByInit) continue;
		if (!client.permit(labelOf(manifest.entries[index])))
			printf("rs: pm refused a label for %s\n", manifest.entries[index].name);
	}
	return true;
}

bool isInitEntry(const Entry &candidate) {
	return strcmp(candidate.name, INIT_PROGRAM_NAME) == 0;
}

uint32_t flagsOf(const Entry &candidate, bool notify) {
	return (notify ? cock::abi::pm::FORWARD_EXIT : NO_FLAGS) |
		   (isInitEntry(candidate) ? cock::abi::pm::AS_INIT : NO_FLAGS);
}

void startedForInit(uint32_t pid, uint32_t label) {
	size_t index = label - cock::abi::service::PROGRAM_SERVICE_BASE;
	if (label < cock::abi::service::PROGRAM_SERVICE_BASE || index >= manifest.count ||
		!manifest.entries[index].startedByInit)
		return;

	const Entry &started = manifest.entries[index];
	bindService(label, pid);
	equip(started, pid);
	printf("rs: started %s for init\n", started.name);
}

const Entry *pmEntry() {
	return entry;
}

cock::user::pm::Client &pmClient() {
	return client;
}

bool registerProcess(uint32_t pid) {
	return recordParent(pid, cock::user::self());
}

bool isExitNotice(const cock::ipc::Message &message) {
	return message.type == cock::ipc::MessageType::PM_EXITED && process != NO_PROCESS &&
		   message.badge == process;
}

bool isSpawnNotice(const cock::ipc::Message &message) {
	return message.type == cock::ipc::MessageType::PM_SPAWNED && process != NO_PROCESS &&
		   message.badge == process;
}

} // namespace cock::rs
