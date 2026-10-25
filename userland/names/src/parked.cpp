#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <names/connect.hpp>
#include <names/parked.hpp>
#include <names/paths.hpp>
#include <string.h>

namespace cock::names {

namespace {

struct Slot {
	bool used = false;
	uint32_t ticket = 0;
	uint32_t process = 0;
	char path[MAX_PATH_BYTES + 1] = {};
};

Slot slots[MAX_PARKED];

Slot *freeSlot() {
	for (Slot &slot : slots)
		if (!slot.used) return &slot;
	for (Slot &slot : slots)
		if (!cock::user::process_alive(slot.process)) return &slot;
	return nullptr;
}

} // namespace

bool park(uint32_t process, const char *path) {
	Slot *slot = freeSlot();
	if (slot == nullptr) return false;

	uint32_t ticket = cock::user::ipc_defer();
	if (ticket == cock::user::NO_TICKET) return false;

	*slot = Slot();
	slot->used = true;
	slot->ticket = ticket;
	slot->process = process;
	memcpy(slot->path, path, strlen(path) + 1);
	return true;
}

void answerParked(uint32_t process) {
	for (Slot &slot : slots) {
		if (!slot.used || slot.process != process) continue;

		uint32_t reply[cock::user::CALL_WORDS] = {};
		abi::names::Status status = connect(process, slot.path, reply);
		reply[abi::names::HEADER_WORD] = static_cast<uint32_t>(status);
		cock::user::ipc_reply_to(slot.ticket, reply);
		slot = Slot();
	}
}

} // namespace cock::names
