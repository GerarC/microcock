#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>

namespace cock::core::memory {

using hal::InterruptGuard;
using task::Process;

SharedMemory::Object SharedMemory::objects[SharedMemory::MAX_OBJECTS] = {};

SharedMemory::Object *SharedMemory::lookup(uint32_t id) {
	if (id == NO_SHARED_OBJECT || id > MAX_OBJECTS) return nullptr;

	Object &object = objects[id - 1];
	return object.used ? &object : nullptr;
}

SharedMemory::Holder *SharedMemory::findHolder(Object &object, uint32_t pid) {
	for (size_t idx = 0; idx < MAX_HOLDERS; idx++) {
		if (object.holders[idx].used && object.holders[idx].pid == pid)
			return &object.holders[idx];
	}
	return nullptr;
}

void SharedMemory::freeIfUnused(Object &object) {
	if (object.mappings != 0) return;
	for (size_t idx = 0; idx < MAX_HOLDERS; idx++) {
		if (object.holders[idx].used) return;
	}

	VirtualMemoryManager::freeFrames(object.frames, object.pages);

	if (object.payer != nullptr) object.payer->returnPages(object.pages);

	object = {};
}

uint32_t SharedMemory::create(Process &creator, size_t pages) {
	if (pages == 0 || pages > MAX_OBJECT_PAGES) return NO_SHARED_OBJECT;

	InterruptGuard guard;
	for (size_t slot = 0; slot < MAX_OBJECTS; slot++) {
		Object &object = objects[slot];
		if (object.used) continue;

		if (!creator.reservePages(pages)) return NO_SHARED_OBJECT;
		if (!VirtualMemoryManager::allocFrames(object.frames, pages)) {
			creator.returnPages(pages);
			return NO_SHARED_OBJECT;
		}

		object.used = true;
		object.payer = &creator;
		object.pages = pages;
		object.mappings = 0;
		object.holders[0] = {true, true, creator.getId()};
		return static_cast<uint32_t>(slot) + 1;
	}
	return NO_SHARED_OBJECT;
}

bool SharedMemory::share(uint32_t id, uint32_t from_pid, uint32_t target_pid,
						 bool writable) {
	InterruptGuard guard;
	Object *object = lookup(id);
	if (object == nullptr) return false;

	Holder *giver = findHolder(*object, from_pid);
	if (giver == nullptr || (writable && !giver->writable)) return false;

	Holder *existing = findHolder(*object, target_pid);
	if (existing != nullptr) {
		existing->writable = existing->writable || writable;
		return true;
	}

	for (size_t idx = 0; idx < MAX_HOLDERS; idx++) {
		if (object->holders[idx].used) continue;
		object->holders[idx] = {true, writable, target_pid};
		return true;
	}
	return false;
}

bool SharedMemory::release(uint32_t id, uint32_t pid) {
	InterruptGuard guard;
	Object *object = lookup(id);
	if (object == nullptr) return false;

	Holder *holder = findHolder(*object, pid);
	if (holder == nullptr) return false;

	*holder = {};
	freeIfUnused(*object);
	return true;
}

void SharedMemory::dropProcess(Process &process) {
	InterruptGuard guard;
	for (size_t slot = 0; slot < MAX_OBJECTS; slot++) {
		Object &object = objects[slot];
		if (!object.used) continue;

		if (object.payer == &process) object.payer = nullptr;
		Holder *holder = findHolder(object, process.getId());
		if (holder == nullptr) continue;

		*holder = {};
		freeIfUnused(object);
	}
}

bool SharedMemory::mayMap(uint32_t id, uint32_t pid, bool writable) {
	InterruptGuard guard;
	Object *object = lookup(id);
	if (object == nullptr) return false;

	Holder *holder = findHolder(*object, pid);
	return holder != nullptr && (!writable || holder->writable);
}

size_t SharedMemory::pageCount(uint32_t id) {
	InterruptGuard guard;
	Object *object = lookup(id);
	return object != nullptr ? object->pages : 0;
}

PhysAddr SharedMemory::frame(uint32_t id, size_t index) {
	InterruptGuard guard;
	Object *object = lookup(id);
	if (object == nullptr || index >= object->pages) return PhysAddr();
	return object->frames[index];
}

void SharedMemory::mapped(uint32_t id) {
	InterruptGuard guard;
	Object *object = lookup(id);
	if (object != nullptr) object->mappings++;
}

void SharedMemory::unmapped(uint32_t id) {
	InterruptGuard guard;
	Object *object = lookup(id);
	if (object == nullptr || object->mappings == 0) return;

	object->mappings--;
	freeIfUnused(*object);
}

size_t SharedMemory::objectCount() {
	InterruptGuard guard;
	size_t count = 0;
	for (size_t slot = 0; slot < MAX_OBJECTS; slot++) {
		if (objects[slot].used) count++;
	}
	return count;
}

} // namespace cock::core::memory
