#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <init/grant.hpp>
#include <init/spawn.hpp>
#include <init/watch.hpp>
#include <stdio.h>

using cock::abi::manifest::Entry;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;
using cock::user::fs::Client;

namespace {

struct Watch {
	const Entry *entry;
	uint32_t pid;
	uint32_t used;
	uint32_t windowStart;
};

constexpr uint32_t NO_USED = 0;
constexpr uint32_t CLOCK_START = 0;

Watch watches[cock::abi::manifest::MAX_ENTRIES];
size_t watchCount = 0;
uint32_t elapsedMilliseconds = CLOCK_START;

Watch *find(uint32_t pid) {
	for (size_t index = 0; index < watchCount; index++)
		if (watches[index].pid == pid) return &watches[index];
	return nullptr;
}

void drop(Watch *watch) {
	*watch = watches[--watchCount];
}

void restart(Client &client, Watch *watch) {
	const Entry &entry = *watch->entry;
	if (elapsedMilliseconds - watch->windowStart >= cock::init::RESTART_WINDOW_MILLISECONDS) {
		watch->windowStart = elapsedMilliseconds;
		watch->used = NO_USED;
	}
	if (watch->used >= entry.restarts) {
		printf("init: %s gave up after %u restarts\n", entry.name,
			   static_cast<unsigned>(watch->used));
		drop(watch);
		return;
	}

	watch->used++;
	uint32_t pid = cock::init::spawnProgram(client, entry, true);
	if (pid == NO_PROCESS) {
		printf("init: %s gave up, cannot start it\n", entry.name);
		drop(watch);
		return;
	}
	cock::init::equip(entry, pid);
	watch->pid = pid;
	printf("init: restarted %s %u of %u\n", entry.name, static_cast<unsigned>(watch->used),
		   static_cast<unsigned>(entry.restarts));
}

void handle(Client &client, const Message &message) {
	if (message.type != MessageType::PROCESS_EXITED) return;
	Watch *watch = find(message.payload.args.arg0);
	if (watch == nullptr) return;
	printf("init: %s ended with status %u\n", watch->entry->name,
		   static_cast<unsigned>(message.payload.args.arg1));
	restart(client, watch);
}

} // namespace

namespace cock::init {

bool watchProgram(const Entry &entry, uint32_t pid) {
	if (entry.restarts == cock::abi::manifest::NO_RESTARTS ||
		watchCount == cock::abi::manifest::MAX_ENTRIES)
		return false;
	watches[watchCount++] = {&entry, pid, NO_USED, elapsedMilliseconds};
	return true;
}

bool anyWatched() {
	return watchCount != 0;
}

void superviseServers(Client &client) {
	Message message = {};
	while (watchCount != 0) {
		if (cock::user::take_stashed(message)) {
			handle(client, message);
			continue;
		}
		cock::user::Received received =
			cock::user::ipc_recv_timeout(message, WATCH_POLL_MILLISECONDS);
		if (received == cock::user::Received::FAILED) return;
		if (received == cock::user::Received::EMPTY)
			elapsedMilliseconds += WATCH_POLL_MILLISECONDS;
		else
			handle(client, message);
	}
}

} // namespace cock::init
