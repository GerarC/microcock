#include <cock/user/api.hpp>
#include <pm/fork.hpp>
#include <pm/state.hpp>

using namespace cock::abi::pm;

namespace cock::pm {

Status forkProcess(uint32_t caller, uint32_t &pid) {
	pid = NO_PID;
	Row *parent = table.findKernel(caller);
	if (parent == nullptr || parent->state != State::RUNNING) return Status::NOT_FOUND;
	if (!table.hasRoom()) return Status::NO_ROOM;

	uint32_t childReply[CALL_WORDS] = {};
	childReply[HEADER_WORD] = packReply(Status::OK);
	childReply[FIRST_WORD] = NO_PID;
	uint32_t created = cock::user::proc_fork(caller, childReply);
	if (created == cock::user::NO_PROCESS) return Status::FAILED;

	Row *row = table.add(created, parent->pid, false);
	if (row == nullptr) return Status::FAILED;
	pid = row->pid;
	return Status::OK;
}

} // namespace cock::pm
