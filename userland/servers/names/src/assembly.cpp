#include <names/assembly.hpp>
#include <string.h>

namespace cock::names {

namespace {

struct Slot {
	bool used = false;
	uint32_t caller = 0;
	size_t length = 0;
	size_t next = 0;
	char text[abi::names::MAX_NAME_BYTES + 1] = {};
};

Slot slots[MAX_ASSEMBLIES];

Slot *find(uint32_t caller) {
	for (Slot &slot : slots)
		if (slot.used && slot.caller == caller) return &slot;
	return nullptr;
}

Slot *take(AliveFunction alive) {
	for (Slot &slot : slots)
		if (!slot.used) return &slot;
	for (Slot &slot : slots)
		if (!alive(slot.caller)) return &slot;
	return nullptr;
}

void release(Slot *slot) {
	if (slot != nullptr) *slot = Slot();
}

} // namespace

Assembled addPart(uint32_t caller, uint32_t header,
				  const uint32_t (&words)[abi::names::CALL_WORDS], AliveFunction alive,
				  bool allowEmpty, char (&name)[abi::names::MAX_NAME_BYTES + 1]) {
	size_t part = abi::names::partOf(header);
	bool last = abi::names::isLast(header);

	Slot *slot = find(caller);
	if (part == 0) {
		if (slot == nullptr) slot = take(alive);
		if (slot == nullptr) return Assembled::BAD;
		*slot = Slot();
		slot->used = true;
		slot->caller = caller;
	} else if (slot == nullptr || slot->next != part) {
		release(slot);
		return Assembled::BAD;
	}

	char bytes[abi::names::PART_BYTES];
	size_t count = abi::names::unpackPart(words, bytes);
	if ((!last && count != abi::names::PART_BYTES) ||
		slot->length + count > abi::names::MAX_NAME_BYTES) {
		release(slot);
		return Assembled::BAD;
	}

	memcpy(slot->text + slot->length, bytes, count);
	slot->length += count;
	slot->next++;
	if (!last) return Assembled::MORE;

	if (slot->length == 0 && !allowEmpty) {
		release(slot);
		return Assembled::BAD;
	}
	slot->text[slot->length] = '\0';
	memcpy(name, slot->text, slot->length + 1);
	release(slot);
	return Assembled::DONE;
}

} // namespace cock::names
