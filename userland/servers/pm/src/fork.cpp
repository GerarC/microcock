#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
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
	row->forkPending = true;
	pid = row->pid;
	return Status::OK;
}

bool holdParent(uint32_t caller, uint32_t child) {
	Row *parent = table.findKernel(caller);
	if (parent == nullptr) return false;

	uint32_t ticket = cock::user::ipc_defer();
	if (ticket == cock::user::NO_TICKET) return false;

	parent->settling = true;
	parent->settleTicket = ticket;
	parent->settleChild = child;
	parent->settleAge = NO_AGE;
	return true;
}

namespace {

void answerParent(Row &parent) {
	uint32_t reply[CALL_WORDS] = {};
	reply[HEADER_WORD] = packReply(Status::OK);
	reply[FIRST_WORD] = parent.settleChild;
	parent.settling = false;
	cock::user::ipc_reply_to(parent.settleTicket, reply);
}

} // namespace

void settleFork(uint32_t childKernelId) {
	Row *child = table.findKernel(childKernelId);
	if (child == nullptr) return;

	Row *parent = table.findSettling(child->pid);
	if (parent != nullptr) answerParent(*parent);
}

void ageSettling() {
	table.ageSettling();
	for (Row *row = table.expiredSettling(MAX_SETTLE_TIMEOUTS); row != nullptr;
		 row = table.expiredSettling(MAX_SETTLE_TIMEOUTS))
		answerParent(*row);
}

bool anySettling() {
	return table.anySettling() != nullptr;
}

} // namespace cock::pm
