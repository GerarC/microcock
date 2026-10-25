#include <cock/abi/shm.hpp>
#include <cock/user/api.hpp>
#include <pm/objects.hpp>

using cock::abi::pm::Status;

namespace {

constexpr size_t NO_BYTES = 0;

cock::pm::Object objects[cock::pm::MAX_OBJECTS] = {
	{cock::pm::NO_OBJECT, cock::pm::NO_OBJECT, NO_BYTES}, {cock::pm::NO_OBJECT, cock::pm::NO_OBJECT, NO_BYTES},
	{cock::pm::NO_OBJECT, cock::pm::NO_OBJECT, NO_BYTES}, {cock::pm::NO_OBJECT, cock::pm::NO_OBJECT, NO_BYTES}};

cock::pm::Object *slotOf(uint32_t caller) {
	for (cock::pm::Object &object : objects)
		if (object.id != cock::pm::NO_OBJECT && object.caller == caller) return &object;
	return nullptr;
}

cock::pm::Object *freeSlot() {
	for (cock::pm::Object &object : objects)
		if (object.id == cock::pm::NO_OBJECT) return &object;
	return nullptr;
}

} // namespace

namespace cock::pm {

Status allocateObject(uint32_t caller, uint32_t bytes, uint32_t &id) {
	if (bytes == NO_BYTES || bytes > cock::abi::shm::MAX_OBJECT_BYTES) return Status::BAD_REQUEST;
	dropObject(caller);

	Object *slot = freeSlot();
	if (slot == nullptr) return Status::NO_ROOM;

	size_t pages = (bytes + cock::abi::shm::PAGE_BYTES - 1) / cock::abi::shm::PAGE_BYTES;
	uint32_t object = cock::user::shm_create(pages);
	if (object == cock::user::NO_SHM) return Status::NO_ROOM;
	if (!cock::user::shm_share(object, caller, true)) {
		cock::user::shm_release(object);
		return Status::FAILED;
	}

	*slot = Object{caller, object, pages * cock::abi::shm::PAGE_BYTES};
	id = object;
	return Status::OK;
}

const Object *findObject(uint32_t caller, uint32_t id) {
	const Object *slot = slotOf(caller);
	return slot != nullptr && slot->id == id ? slot : nullptr;
}

void dropObject(uint32_t caller) {
	Object *slot = slotOf(caller);
	if (slot == nullptr) return;
	cock::user::shm_release(slot->id);
	*slot = Object{NO_OBJECT, NO_OBJECT, NO_BYTES};
}

} // namespace cock::pm
