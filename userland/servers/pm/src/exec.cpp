#include <cock/abi/exec.hpp>
#include <cock/abi/executable.hpp>
#include <cock/elf/image.hpp>
#include <cock/exec/args.hpp>
#include <cock/user/api.hpp>
#include <cock/user/spawn.hpp>
#include <pm/exec.hpp>
#include <pm/objects.hpp>
#include <pm/state.hpp>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>

using cock::abi::pm::ExecRequest;
using cock::abi::pm::Status;

namespace {

constexpr size_t NO_BYTES = 0;
constexpr size_t ONE_PAGE = 1;

bool nameEnds(const ExecRequest &request) {
	for (size_t index = 0; index < cock::abi::pm::NAME_BYTES; index++)
		if (request.name[index] == '\0') return index != NO_BYTES;
	return false;
}

bool valid(const ExecRequest &request, size_t objectBytes) {
	if (!nameEnds(request) || !cock::exec::valid(request.args)) return false;
	if (request.imageBytes == NO_BYTES || request.imageBytes > cock::abi::pm::MAX_EXEC_IMAGE_BYTES)
		return false;
	return cock::abi::pm::IMAGE_OFFSET + request.imageBytes <= objectBytes;
}

bool buildImage(uint32_t handle, const uint8_t *image, const cock::elf::Image &parsed,
				const cock::abi::exec::ArgsBlock &args) {
	if (!cock::user::load_segments(handle, image, parsed)) return false;
	if (!cock::user::proc_map(handle, cock::abi::exec::ARGS_ADDRESS, ONE_PAGE)) return false;
	return cock::user::proc_write(handle, cock::abi::exec::ARGS_ADDRESS, &args, sizeof(args));
}

} // namespace

namespace cock::pm {

Status execFromObject(uint32_t caller, uint32_t id, bool &replaced) {
	replaced = false;
	Row *row = table.findKernel(caller);
	if (row == nullptr || row->state != cock::abi::pm::State::RUNNING) return Status::NOT_FOUND;

	const Object *object = findObject(caller, id);
	if (object == nullptr) return Status::BAD_REQUEST;
	size_t objectBytes = object->bytes;

	void *base = cock::user::shm_map(id, false);
	if (base == nullptr) {
		dropObject(caller);
		return Status::NO_ROOM;
	}

	ExecRequest request;
	memcpy(&request, base, sizeof(request));
	const uint8_t *image = static_cast<const uint8_t *>(base) + cock::abi::pm::IMAGE_OFFSET;

	Status status = Status::BAD_REQUEST;
	cock::elf::Image parsed;
	if (valid(request, objectBytes) &&
		cock::elf::parse(image, request.imageBytes, cock::abi::exec::IMAGE_LIMIT, parsed)) {
		status = Status::FAILED;
		uint32_t handle = cock::user::proc_create(request.name, false, nullptr,
												  cock::abi::executable::USER_SPACE_LIMIT);
		if (handle != cock::abi::process::NO_PENDING_PROCESS) {
			if (buildImage(handle, image, parsed, request.args) &&
				cock::user::proc_exec(handle, parsed.entry, caller, cock::abi::exec::ARGS_ADDRESS)) {
				replaced = true;
				status = Status::OK;
				Row *row = table.findKernel(caller);
				if (row != nullptr) copyName(row->name, request.name);
			} else {
				cock::user::proc_discard(handle);
			}
		}
	}

	munmap(base, objectBytes);
	dropObject(caller);
	return status;
}

} // namespace cock::pm
