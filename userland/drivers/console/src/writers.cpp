#include <cock/user/api.hpp>
#include <console/display.hpp>
#include <console/serial.hpp>
#include <console/writers.hpp>

namespace cock::console {

namespace {

constexpr size_t SLOT_COUNT = 8;
constexpr size_t SLOT_BYTES = 256;
constexpr size_t NO_LENGTH = 0;

struct Slot {
	bool used;
	uint32_t sender;
	uint32_t terminal;
	size_t length;
	char bytes[SLOT_BYTES];
};

Slot slots[SLOT_COUNT];

void emit(const char *text, size_t length, uint32_t terminal) {
	serialWrite(text, length, terminal);
	screenWrite(text, length, terminal);
}

void flush(Slot &slot) {
	emit(slot.bytes, slot.length, slot.terminal);
	slot.length = NO_LENGTH;
	slot.used = false;
}

Slot *findSlot(uint32_t sender) {
	Slot *free = nullptr;
	for (Slot &slot : slots) {
		if (slot.used && slot.sender == sender) return &slot;
		if (!slot.used && free == nullptr) free = &slot;
	}
	if (free != nullptr) return free;

	for (Slot &slot : slots) {
		if (cock::user::process_alive(slot.sender)) continue;
		flush(slot);
		return &slot;
	}
	return nullptr;
}

} // namespace

void handleWrite(uint32_t sender, const char *bytes, size_t length, bool last, uint32_t terminal) {
	Slot *slot = findSlot(sender);
	if (slot == nullptr) return emit(bytes, length, terminal);

	if (slot->length + length > SLOT_BYTES) {
		emit(slot->bytes, slot->length, slot->terminal);
		slot->length = NO_LENGTH;
	}
	for (size_t index = 0; index < length; index++)
		slot->bytes[slot->length + index] = bytes[index];
	slot->length += length;
	slot->used = true;
	slot->sender = sender;
	slot->terminal = terminal;
	if (last) flush(*slot);
}

bool hasPartialText() {
	for (const Slot &slot : slots) {
		if (slot.used && slot.length > NO_LENGTH) return true;
	}
	return false;
}

void sweepDeadWriters() {
	for (Slot &slot : slots) {
		if (!slot.used || cock::user::process_alive(slot.sender)) continue;
		flush(slot);
	}
}

} // namespace cock::console
