#include <cock/user/api.hpp>
#include <init/keep.hpp>
#include <init/launch.hpp>
#include <init/manifest.hpp>
#include <stdio.h>

using cock::user::NO_PROCESS;

namespace {

constexpr uint32_t NO_ATTEMPTS = 0;

struct Kept {
	size_t index;
	uint32_t pid;
	uint32_t attempts;
	bool gaveUp;
};

Kept kept[cock::abi::manifest::MAX_ENTRIES];
size_t keptCount = 0;

bool stillOnTheLoose() {
	for (size_t position = 0; position < keptCount; position++)
		if (!kept[position].gaveUp) return true;
	return false;
}

void look(cock::user::fs::Client &files, cock::user::pm::Client &process, Kept &program) {
	if (program.gaveUp) return;
	if (program.pid != NO_PROCESS && cock::user::process_alive(program.pid)) return;
	if (program.pid != NO_PROCESS) process.wait();

	const cock::abi::manifest::Entry &entry = cock::init::manifest.entries[program.index];
	if (program.attempts > cock::init::MAX_RESTARTS) {
		printf("init: %s gave up after %u restarts\n", entry.name,
			   static_cast<unsigned>(cock::init::MAX_RESTARTS));
		program.gaveUp = true;
		return;
	}
	if (program.pid != NO_PROCESS)
		printf("init: %s ended, starting it again %u of %u\n", entry.name,
			   static_cast<unsigned>(program.attempts), static_cast<unsigned>(cock::init::MAX_RESTARTS));

	program.attempts++;
	program.pid = cock::init::launch(files, process, entry, program.index);
}

} // namespace

namespace cock::init {

void keepRunning(cock::user::fs::Client &files, cock::user::pm::Client &process) {
	for (size_t index = 0; index < manifest.count; index++)
		if (manifest.entries[index].startedByInit)
			kept[keptCount++] = Kept{index, NO_PROCESS, NO_ATTEMPTS, false};

	for (size_t position = 0; position < keptCount; position++) look(files, process, kept[position]);
	while (stillOnTheLoose()) {
		cock::user::sleep_ms(KEEP_POLL_MILLISECONDS);
		for (size_t position = 0; position < keptCount; position++) look(files, process, kept[position]);
	}
}

} // namespace cock::init
