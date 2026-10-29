#include <cock/pm/classify.hpp>
#include <cock/user/api.hpp>
#include <pm/objects.hpp>
#include <pm/snapshot.hpp>
#include <string.h>
#include <sys/mman.h>

using cock::abi::pm::ProcessRecord;
using cock::abi::pm::Snapshot;
using cock::abi::pm::State;
using cock::abi::pm::Status;

namespace cock::pm {

State visibleState(const Row &row) {
	if (row.state != State::RUNNING) return row.state;
	State seen = classify(cock::user::process_state_mask(row.kernelId));
	return seen != State::FREE ? seen : row.state;
}

Status takeSnapshot(uint32_t caller, uint32_t id, uint32_t &count) {
	const Object *object = findObject(caller, id);
	if (object == nullptr) return Status::BAD_REQUEST;
	size_t objectBytes = object->bytes;
	if (objectBytes < sizeof(Snapshot)) {
		dropObject(caller);
		return Status::BAD_REQUEST;
	}

	void *base = cock::user::shm_map(id, true);
	if (base == nullptr) {
		dropObject(caller);
		return Status::NO_ROOM;
	}

	Snapshot *snapshot = static_cast<Snapshot *>(base);
	uint32_t rows = 0;
	for (size_t index = 0; index < cock::abi::pm::MAX_PROCESSES; index++) {
		const Row *row = table.at(index);
		if (row == nullptr) break;
		ProcessRecord &record = snapshot->records[rows++];
		record.kernelId = row->kernelId;
		record.pid = row->pid;
		record.parent = row->parent;
		record.state = visibleState(*row);
		record.exitStatus = row->status;
		memcpy(record.name, row->name, sizeof(record.name));
	}
	snapshot->count = rows;

	munmap(base, objectBytes);
	dropObject(caller);
	count = rows;
	return Status::OK;
}

} // namespace cock::pm
