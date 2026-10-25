#include <cock/pm/classify.hpp>
#include <cock/user/api.hpp>
#include <pm/objects.hpp>
#include <pm/requests.hpp>
#include <pm/spawn.hpp>
#include <pm/state.hpp>
#include <pm/wait.hpp>

using namespace cock::abi::pm;
using cock::pm::Caller;

namespace {

constexpr uint32_t NO_VALUE = 0;

Status allocate(const uint32_t (&words)[CALL_WORDS], uint32_t caller, uint32_t (&reply)[CALL_WORDS]) {
	uint32_t id = NO_VALUE;
	Status status = cock::pm::allocateObject(caller, words[FIRST_WORD], id);
	reply[FIRST_WORD] = id;
	return status;
}

Status spawnProcess(const uint32_t (&words)[CALL_WORDS], uint32_t caller, uint32_t (&reply)[CALL_WORDS]) {
	uint32_t kernelId = NO_VALUE;
	uint32_t pid = NO_PID;
	Status status = cock::pm::spawnFromObject(caller, words[FIRST_WORD], kernelId, pid);
	reply[FIRST_WORD] = kernelId;
	reply[SECOND_WORD] = pid;
	return status;
}

Status registerProcess(const uint32_t (&words)[CALL_WORDS], uint32_t (&reply)[CALL_WORDS]) {
	uint32_t kernelId = words[FIRST_WORD];
	uint32_t parentKernel = words[SECOND_WORD];
	uint32_t wanted = words[THIRD_WORD];
	if (!cock::user::process_alive(kernelId)) return Status::BAD_REQUEST;

	uint32_t parent = NO_PID;
	if (parentKernel != NO_PARENT) {
		cock::pm::Row *row = cock::pm::table.findKernel(parentKernel);
		if (row == nullptr) return Status::NOT_FOUND;
		parent = row->pid;
	}
	if (!cock::pm::table.hasRoom()) return Status::NO_ROOM;

	cock::pm::Row *row = cock::pm::table.add(kernelId, parent, false, wanted);
	if (row == nullptr) return Status::BAD_REQUEST;
	if (kernelId == cock::abi::service::ROOT_PID) cock::pm::table.setReaper(row->pid);
	reply[FIRST_WORD] = row->pid;
	return Status::OK;
}

Status list(const uint32_t (&words)[CALL_WORDS], uint32_t (&reply)[CALL_WORDS], State &state) {
	const cock::pm::Row *row = cock::pm::table.at(words[FIRST_WORD]);
	if (row == nullptr) return Status::NOT_FOUND;
	reply[FIRST_WORD] = row->kernelId;
	reply[SECOND_WORD] = row->pid;
	reply[THIRD_WORD] = row->parent;
	state = row->state;
	if (state == State::RUNNING) {
		State seen = cock::pm::classify(cock::user::process_state_mask(row->kernelId));
		if (seen != State::FREE) state = seen;
	}
	return Status::OK;
}

Status permitLabel(const uint32_t (&words)[CALL_WORDS]) {
	return cock::pm::permit(words[FIRST_WORD]) ? Status::OK : Status::BAD_REQUEST;
}

bool allowed(Op op, Caller who) {
	switch (op) {
	case Op::LIST:
	case Op::WAIT:
		return true;
	case Op::ALLOCATE:
	case Op::SPAWN:
		return who == Caller::ROOT || who == Caller::INIT;
	case Op::REGISTER:
	case Op::PERMIT:
		return who == Caller::ROOT;
	}
	return false;
}

bool knownOp(uint32_t op) {
	return op >= static_cast<uint32_t>(Op::ALLOCATE) && op <= static_cast<uint32_t>(Op::WAIT);
}

} // namespace

namespace cock::pm {

bool serve(uint32_t caller, const uint32_t (&words)[CALL_WORDS], uint32_t (&reply)[CALL_WORDS]) {
	for (uint32_t &word : reply) word = NO_VALUE;

	Status status = Status::BAD_REQUEST;
	State state = State::FREE;
	bool detached = false;
	uint32_t number = opOf(words[HEADER_WORD]);
	if (knownOp(number)) {
		Op op = static_cast<Op>(number);
		if (!allowed(op, callerOf(caller))) {
			status = Status::DENIED;
		} else {
			switch (op) {
			case Op::ALLOCATE:
				status = allocate(words, caller, reply);
				break;
			case Op::SPAWN:
				status = spawnProcess(words, caller, reply);
				break;
			case Op::REGISTER:
				status = registerProcess(words, reply);
				break;
			case Op::LIST:
				status = list(words, reply, state);
				break;
			case Op::PERMIT:
				status = permitLabel(words);
				break;
			case Op::WAIT:
				status = waitForChild(caller, words[FIRST_WORD], reply, detached);
				break;
			}
		}
	}
	if (detached) return false;
	reply[HEADER_WORD] = packReply(status, state);
	return true;
}

} // namespace cock::pm
