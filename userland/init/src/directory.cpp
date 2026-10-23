#include <cock/abi/capability.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/names.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <init/directory.hpp>
#include <init/grant.hpp>
#include <init/manifest.hpp>
#include <init/service.hpp>
#include <stdio.h>
#include <string.h>

using cock::abi::CapabilityType;
using cock::abi::manifest::Entry;
using cock::abi::names::Op;
using cock::abi::names::Status;
using cock::abi::service::NAMES_PID;
using cock::abi::service::NAMES_SERVICE;
using cock::abi::service::NO_SERVICE;
using cock::abi::service::RAMFS_SERVICE;
using cock::user::CALL_WORDS;

namespace {

constexpr const char NAMES_MODULE[] = "names";
constexpr uintptr_t FIRST_LABEL = 0;
constexpr size_t NO_LENGTH = 0;
constexpr size_t SINGLE_PART = 1;

bool ask(const uint32_t (&words)[CALL_WORDS]) {
	cock::user::CallResult result = cock::user::ipc_call(NAMES_PID, words);
	return result.ok &&
		   static_cast<Status>(result.words[cock::abi::names::HEADER_WORD]) == Status::OK;
}

bool define(uint32_t label, const char *name) {
	size_t length = strlen(name);
	if (length > cock::abi::names::MAX_NAME_BYTES) {
		printf("init: %s cannot be reached by name (up to %u bytes)\n", name,
			   static_cast<unsigned>(cock::abi::names::MAX_NAME_BYTES));
		length = NO_LENGTH;
	}

	size_t parts = length == NO_LENGTH ? SINGLE_PART : cock::abi::names::partCount(length);
	for (size_t part = 0; part < parts; part++) {
		bool last = part + 1 == parts;
		uint32_t words[CALL_WORDS] = {cock::abi::names::pack(Op::DEFINE, label, part, last)};
		cock::abi::names::packPart(name, length, part, words);

		cock::user::CallResult result = cock::user::ipc_call(NAMES_PID, words);
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

} // namespace

namespace cock::init {

bool startNames() {
	uint32_t names = cock::user::module_process(NAMES_MODULE);
	if (names != NAMES_PID) {
		printf("init: no name server\n");
		return false;
	}

	if (!cock::user::service_set(names, NAMES_SERVICE) ||
		!cock::user::cap_grant(names, CapabilityType::IPC, FIRST_LABEL,
							   cock::abi::service::GRANTABLE_LABEL_COUNT, true)) {
		printf("init: cannot equip the name server\n");
		return false;
	}
	announce(names);

	if (!define(RAMFS_SERVICE, cock::abi::fs::RAMFS_SERVER_NAME)) {
		printf("init: cannot register %s\n", cock::abi::fs::RAMFS_SERVER_NAME);
		return false;
	}
	return true;
}

bool describeSystem() {
	bool accepted = true;
	for (size_t index = 0; index < manifest.count; index++) {
		const Entry &entry = manifest.entries[index];
		if (!define(labelOf(entry), entry.name)) {
			printf("init: cannot register %s\n", entry.name);
			accepted = false;
		}
	}

	for (size_t index = 0; index < manifest.count; index++) {
		const Entry &entry = manifest.entries[index];
		for (size_t peer = 0; peer < entry.ipcCount; peer++) {
			uint32_t label = labelOfName(entry.ipc[peer]);
			if (label == NO_SERVICE || !allow(labelOf(entry), label)) {
				printf("init: %s cannot talk to %s\n", entry.name, entry.ipc[peer]);
				accepted = false;
			}
		}
	}
	return accepted;
}

bool bindService(uint32_t label, uint32_t pid) {
	uint32_t words[CALL_WORDS] = {cock::abi::names::pack(Op::BIND, label), pid};
	if (ask(words)) return true;
	if (!cock::user::process_alive(pid)) return false;
	printf("init: cannot bind process %u to label %u\n", static_cast<unsigned>(pid),
		   static_cast<unsigned>(label));
	return false;
}

} // namespace cock::init
