#include <cock/abi/capability.hpp>
#include <cock/user/api.hpp>
#include <names/grants.hpp>

namespace cock::names {

namespace {

constexpr uint32_t NO_CLIENT = 0xFFFFFFFF;

struct Entry {
	uint32_t client = NO_CLIENT;
	uint32_t label = 0;
	uint32_t handle = cock::abi::NO_CAPABILITY_NODE;
};

Entry entries[MAX_GRANTS];

void dropEnded() {
	for (Entry &entry : entries)
		if (entry.client != NO_CLIENT && !cock::user::process_alive(entry.client))
			entry = Entry();
}

} // namespace

bool granted(uint32_t client, uint32_t label) {
	for (const Entry &entry : entries)
		if (entry.client == client && entry.label == label) return true;
	return false;
}

uint32_t handleOf(uint32_t client, uint32_t label) {
	for (const Entry &entry : entries)
		if (entry.client == client && entry.label == label) return entry.handle;
	return cock::abi::NO_CAPABILITY_NODE;
}

bool hasRoom() {
	for (const Entry &entry : entries)
		if (entry.client == NO_CLIENT) return true;

	dropEnded();
	for (const Entry &entry : entries)
		if (entry.client == NO_CLIENT) return true;
	return false;
}

void record(uint32_t client, uint32_t label, uint32_t handle) {
	for (Entry &entry : entries) {
		if (entry.client != NO_CLIENT) continue;
		entry.client = client;
		entry.label = label;
		entry.handle = handle;
		return;
	}
}

} // namespace cock::names
