#include <cock/core/syscall/proc/served_target.hpp>

namespace cock::core::syscall {

using task::Thread;
using task::ThreadState;

namespace {

bool is_served_by(const Thread &server, const Thread &caller) {
	if (server.serving == &caller) return true;
	for (const Thread *waiting = server.deferred; waiting != nullptr; waiting = waiting->callLink)
		if (waiting == &caller) return true;
	return false;
}

} // namespace

bool serves_main_thread(const Thread &server, const Thread *target) {
	if (target == nullptr || !target->isMainThread()) return false;
	task::Process *process = target->getProcess();
	if (process->isKernel() || process == server.getProcess()) return false;
	return target->getState() == ThreadState::WAITING_REPLY && is_served_by(server, *target);
}

} // namespace cock::core::syscall
