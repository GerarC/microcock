#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/task/pending_process.hpp>
#include <string.h>

namespace cock::core::task {

using hal::InterruptGuard;

PendingProcesses::Entry PendingProcesses::entries[CAPACITY] = {};

bool PendingProcesses::canAdd(const Process *owner) {
	InterruptGuard guard;
	size_t held = 0;
	bool room = false;
	for (size_t index = 0; index < CAPACITY; index++) {
		if (entries[index].process == nullptr) room = true;
		else if (entries[index].owner == owner) held++;
	}
	return room && held < abi::process::MAX_PENDING_PER_PROCESS;
}

uint32_t PendingProcesses::add(const Process *owner, Process *process, bool notify,
							   const abi::service::SpawnRights *rights) {
	if (!canAdd(owner)) return NO_HANDLE;

	InterruptGuard guard;
	for (size_t index = 0; index < CAPACITY; index++) {
		if (entries[index].process != nullptr) continue;
		entries[index] = {};
		entries[index].process = process;
		entries[index].owner = owner;
		entries[index].notify = notify;
		entries[index].hasRights = rights != nullptr;
		if (rights != nullptr) memcpy(&entries[index].rights, rights, sizeof(*rights));
		return static_cast<uint32_t>(index);
	}
	return NO_HANDLE;
}

PendingProcesses::Entry *PendingProcesses::find(uint32_t handle, const Process *owner) {
	if (handle >= CAPACITY) return nullptr;
	Entry &entry = entries[handle];
	if (entry.process == nullptr || entry.owner != owner) return nullptr;
	return &entry;
}

void PendingProcesses::release(uint32_t handle) {
	if (handle >= CAPACITY) return;
	InterruptGuard guard;
	entries[handle] = {};
}

void PendingProcesses::discard(uint32_t handle) {
	if (handle >= CAPACITY) return;

	Process *process = nullptr;
	{
		InterruptGuard guard;
		process = entries[handle].process;
		entries[handle] = {};
	}
	delete process;
}

void PendingProcesses::discardOwnedBy(const Process *owner) {
	for (size_t index = 0; index < CAPACITY; index++) {
		if (entries[index].process != nullptr && entries[index].owner == owner)
			discard(static_cast<uint32_t>(index));
	}
}

} // namespace cock::core::task
