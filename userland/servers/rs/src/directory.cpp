#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/names.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <rs/directory.hpp>
#include <rs/endpoints.hpp>
#include <rs/forks.hpp>
#include <rs/grant.hpp>
#include <rs/manifest.hpp>
#include <rs/service.hpp>
#include <stdio.h>
#include <string.h>

using cock::abi::CapabilityType;
using cock::abi::manifest::Entry;
using cock::abi::names::Op;
using cock::abi::names::Status;
using cock::abi::service::NAMES_SERVICE;
using cock::abi::service::NO_SERVICE;
using cock::abi::service::RAMFS_SERVICE;
using cock::user::CALL_WORDS;

namespace {

constexpr const char NAMES_MODULE[] = "names";
constexpr size_t NO_LENGTH = 0;
constexpr size_t SINGLE_PART = 1;

uint32_t namesHandle = cock::abi::NO_CAPABILITY_NODE;

bool ask(const uint32_t (&words)[CALL_WORDS]) {
	cock::user::CallResult result = cock::user::ep_call(namesHandle, words);
	return result.ok &&
		   static_cast<Status>(result.words[cock::abi::names::HEADER_WORD]) == Status::OK;
}

bool define(uint32_t label, const char *name) {
	size_t length = strlen(name);
	if (length > cock::abi::names::MAX_NAME_BYTES) {
		printf("rs: %s cannot be reached by name (up to %u bytes)\n", name,
			   static_cast<unsigned>(cock::abi::names::MAX_NAME_BYTES));
		length = NO_LENGTH;
	}

	size_t parts = length == NO_LENGTH ? SINGLE_PART : cock::abi::names::partCount(length);
	for (size_t part = 0; part < parts; part++) {
		bool last = part + 1 == parts;
		uint32_t words[CALL_WORDS] = {cock::abi::names::pack(Op::DEFINE, label, part, last)};
		cock::abi::names::packPart(name, length, part, words);

		cock::user::CallResult result = cock::user::ep_call(namesHandle, words);
		Status expected = last ? Status::OK : Status::MORE;
		if (!result.ok ||
			static_cast<Status>(result.words[cock::abi::names::HEADER_WORD]) != expected)
			return false;
	}
	return true;
}

bool allow(uint32_t from, uint32_t to) {
	uint32_t words[CALL_WORDS] = {cock::abi::names::pack(Op::ALLOW, from), to};
	return ask(words);
}

bool own(uint32_t service, uint32_t owner, uint32_t mode) {
	uint32_t words[CALL_WORDS] = {cock::abi::names::pack(Op::OWNER, service), owner, mode};
	return ask(words);
}

} // namespace

namespace cock::rs {

bool startNames() {
	uint32_t names = cock::user::module_process(NAMES_MODULE);
	if (names == cock::user::NO_PROCESS) {
		printf("rs: no name server\n");
		return false;
	}

	if (!cock::user::service_set(names, NAMES_SERVICE) ||
		!cock::user::cap_grant(names, CapabilityType::ENDPOINT, cock::abi::endpoint::FIRST_ENDPOINT,
							   cock::abi::endpoint::ENDPOINT_RANGE_COUNT, true, true) ||
		bindEndpoint(NAMES_SERVICE, names) == cock::abi::endpoint::NO_ENDPOINT) {
		printf("rs: cannot equip the name server\n");
		return false;
	}

	namesHandle = handleOf(NAMES_SERVICE);
	if (namesHandle == cock::abi::NO_CAPABILITY_NODE) {
		printf("rs: no handle on the name server\n");
		return false;
	}
	announce(NAMES_SERVICE);

	if (!define(RAMFS_SERVICE, cock::abi::fs::RAMFS_SERVER_NAME)) {
		printf("rs: cannot register %s\n", cock::abi::fs::RAMFS_SERVER_NAME);
		return false;
	}
	return true;
}

bool describeSystem() {
	bool accepted = true;
	for (size_t index = 0; index < manifest.count; index++) {
		const Entry &entry = manifest.entries[index];
		if (!define(labelOf(entry), entry.name)) {
			printf("rs: cannot register %s\n", entry.name);
			accepted = false;
		}
	}

	for (size_t index = 0; index < manifest.count; index++) {
		const Entry &entry = manifest.entries[index];
		for (size_t peer = 0; peer < entry.ipcCount; peer++) {
			uint32_t label = labelOfName(entry.ipc[peer]);
			if (label == NO_SERVICE || !allow(labelOf(entry), label)) {
				printf("rs: %s cannot talk to %s\n", entry.name, entry.ipc[peer]);
				accepted = false;
			}
		}
	}

	for (size_t index = 0; index < manifest.count; index++) {
		const Entry &entry = manifest.entries[index];
		if (!entry.hasOwnership()) continue;

		uint32_t owner = entry.owner[0] == '\0' ? NO_SERVICE : labelOfName(entry.owner);
		if ((entry.owner[0] != '\0' && owner == NO_SERVICE) ||
			!own(labelOf(entry), owner, entry.mode)) {
			printf("rs: %s cannot be owned by %s\n", entry.name, entry.owner);
			accepted = false;
		}
	}
	return accepted;
}

bool bindService(uint32_t label, uint32_t pid) {
	uint32_t endpoint = bindEndpoint(label, pid);
	uint32_t words[CALL_WORDS] = {cock::abi::names::pack(Op::BIND, label), pid, endpoint};
	if (ask(words)) return true;
	if (!cock::user::process_alive(pid)) return false;
	printf("rs: cannot bind process %u to label %u\n", static_cast<unsigned>(pid),
		   static_cast<unsigned>(label));
	return false;
}

bool registerFork(uint32_t child, uint32_t parent) {
	forwardFork(child, parent);

	uint32_t words[CALL_WORDS] = {cock::abi::names::pack(Op::FORKED), child, parent};
	bool registered = ask(words);
	if (!registered && cock::user::process_alive(child))
		printf("rs: cannot register copy %u of %u\n", static_cast<unsigned>(child),
			   static_cast<unsigned>(parent));

	acknowledgeFork(child);
	return registered;
}

} // namespace cock::rs
