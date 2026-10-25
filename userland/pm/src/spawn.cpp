#include <cock/abi/endpoint.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/spawn.hpp>
#include <pm/objects.hpp>
#include <pm/spawn.hpp>
#include <pm/state.hpp>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>

using cock::abi::pm::SpawnRequest;
using cock::abi::pm::Status;
using cock::pm::Caller;

namespace {

constexpr size_t NO_BYTES = 0;
constexpr uint32_t NO_FLAGS = 0;
constexpr uint32_t NO_LABELS = 0;

bool nameEnds(const SpawnRequest &request) {
	for (size_t index = 0; index < cock::abi::pm::NAME_BYTES; index++)
		if (request.name[index] == '\0') return index != NO_BYTES;
	return false;
}

bool labelAllowed(uint32_t label) {
	uint32_t first = cock::abi::service::PROGRAM_SERVICE_BASE;
	return label >= first && label - first < cock::abi::pm::PROGRAM_LABEL_COUNT;
}

bool valid(const SpawnRequest &request, size_t objectBytes) {
	const cock::abi::service::SpawnRights &rights = request.rights;
	if (!nameEnds(request) || (request.flags & ~cock::abi::pm::KNOWN_FLAGS) != 0) return false;
	if (request.imageBytes == NO_BYTES || request.imageBytes > cock::abi::pm::MAX_IMAGE_BYTES)
		return false;
	if (cock::abi::pm::IMAGE_OFFSET + request.imageBytes > objectBytes) return false;
	if (!labelAllowed(rights.label) || rights.ipcCount > cock::abi::service::MAX_SPAWN_IPC) return false;
	for (uint32_t index = 0; index < rights.ipcCount; index++)
		if (!labelAllowed(rights.ipc[index])) return false;
	return true;
}

bool mayCreate(Caller who, SpawnRequest &request) {
	if (who == Caller::ROOT) {
		if ((request.flags & cock::abi::pm::AS_INIT) == NO_FLAGS) return true;
		cock::pm::Row *held = cock::pm::table.find(cock::abi::pm::INIT_PID);
		return held == nullptr || held->state != cock::abi::pm::State::RUNNING;
	}
	if (who != Caller::INIT || request.flags != NO_FLAGS || !cock::pm::permitted(request.rights.label))
		return false;

	request.rights.ipcCount = NO_LABELS;
	request.rights.initialEndpoint = cock::pm::namesEndpoint;
	request.rights.initialFlags = NO_FLAGS;
	return true;
}

} // namespace

namespace cock::pm {

Status spawnFromObject(uint32_t caller, uint32_t id, uint32_t &kernelId, uint32_t &pid) {
	const Object *object = findObject(caller, id);
	if (object == nullptr) return Status::BAD_REQUEST;
	size_t objectBytes = object->bytes;

	void *base = cock::user::shm_map(id, false);
	if (base == nullptr) {
		dropObject(caller);
		return Status::NO_ROOM;
	}

	Status status = Status::BAD_REQUEST;
	Caller who = callerOf(caller);
	SpawnRequest request;
	memcpy(&request, base, sizeof(request));
	if (valid(request, objectBytes) && mayCreate(who, request)) {
		status = Status::NO_ROOM;
		if (table.hasRoom()) {
			const uint8_t *image = static_cast<const uint8_t *>(base) + cock::abi::pm::IMAGE_OFFSET;
			uint32_t created = cock::user::spawn(image, request.imageBytes, request.name, true, &request.rights);
			if (created == cock::user::NO_PROCESS) {
				status = Status::FAILED;
			} else {
				Row *parent = table.findKernel(caller);
				bool forward = (request.flags & cock::abi::pm::FORWARD_EXIT) != NO_FLAGS;
				uint32_t wanted = (request.flags & cock::abi::pm::AS_INIT) != NO_FLAGS
									  ? cock::abi::pm::INIT_PID
									  : cock::abi::pm::NO_PID;
				uint32_t announce = who == Caller::INIT ? request.rights.label : cock::abi::service::NO_SERVICE;
				Row *row = table.add(created, parent == nullptr ? cock::abi::pm::NO_PID : parent->pid, forward,
									 wanted, announce);
				kernelId = created;
				pid = row->pid;
				status = Status::OK;
			}
		}
	}

	munmap(base, objectBytes);
	dropObject(caller);
	return status;
}

} // namespace cock::pm
