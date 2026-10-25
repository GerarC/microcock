#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <pm/state.hpp>
#include <pm/wait.hpp>

using namespace cock::abi::pm;

namespace {

void fill(const cock::pm::Row &child, uint32_t (&words)[CALL_WORDS]) {
	words[HEADER_WORD] = packReply(Status::OK);
	words[FIRST_WORD] = child.pid;
	words[SECOND_WORD] = child.status;
}

} // namespace

namespace cock::pm {

Status waitForChild(uint32_t caller, uint32_t target, uint32_t (&reply)[CALL_WORDS], bool &detached) {
	detached = false;
	Row *parent = table.findKernel(caller);
	if (parent == nullptr) return Status::NOT_FOUND;

	Row *zombie = table.findZombie(parent->pid, target);
	if (zombie != nullptr) {
		fill(*zombie, reply);
		table.collect(*zombie);
		return Status::OK;
	}
	if (!table.hasChild(parent->pid, target)) return Status::NOT_FOUND;

	uint32_t ticket = cock::user::ipc_defer();
	if (ticket == cock::user::NO_TICKET) return Status::FAILED;
	parent->waiting = true;
	parent->waitTicket = ticket;
	parent->waitTarget = target;
	detached = true;
	return Status::OK;
}

void answerWaiter(Row &exited) {
	if (exited.state != State::ZOMBIE) return;
	Row *parent = table.find(exited.parent);
	if (parent == nullptr || !parent->waiting) return;
	if (parent->waitTarget != ANY_CHILD && parent->waitTarget != exited.pid) return;

	uint32_t reply[CALL_WORDS] = {};
	fill(exited, reply);
	parent->waiting = false;
	if (cock::user::ipc_reply_to(parent->waitTicket, reply)) table.collect(exited);
}

} // namespace cock::pm
