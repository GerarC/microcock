#include <cock/user/api.hpp>
#include <init/grant.hpp>
#include <init/run.hpp>
#include <init/service.hpp>
#include <init/spawn.hpp>
#include <init/supervise.hpp>
#include <init/watch.hpp>
#include <stdio.h>

using cock::abi::manifest::Entry;
using cock::user::NO_PROCESS;
using cock::user::fs::Client;

namespace {

constexpr uint32_t NO_NOTICES = 0;

} // namespace

namespace cock::init {

void runEntry(Client &client, const Entry &entry) {
	if (entry.isBootModule()) {
		uint32_t pid = cock::user::module_process(entry.name);
		if (pid == NO_PROCESS) {
			printf("init: no boot module %s\n", entry.name);
			return;
		}
		labelBootModule(entry, pid);
		equip(entry, pid);
		printf("init: configured %s\n", entry.name);
		return;
	}

	bool supervised = entry.repeat > cock::abi::manifest::DEFAULT_REPEAT;
	bool watched = entry.restarts != cock::abi::manifest::NO_RESTARTS;
	uint32_t notices = NO_NOTICES;
	for (uint32_t run = 0; run < entry.repeat; run++) {
		uint32_t pid = spawnProgram(client, entry, supervised || watched);
		if (pid == NO_PROCESS) return;
		equip(entry, pid);
		if (watched) watchProgram(entry, pid);
		if (supervised && awaitExit(pid)) notices++;
	}
	if (supervised)
		printf("init: %s exit notices %u/%u\n", entry.name, static_cast<unsigned>(notices),
			   static_cast<unsigned>(entry.repeat));
}

} // namespace cock::init
