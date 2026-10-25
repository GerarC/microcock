#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <pm/signal.hpp>
#include <pm/state.hpp>

using namespace cock::abi::pm;

namespace {

bool knownSignal(uint32_t signal) {
	return signal == cock::abi::process::SIGNAL_INTERRUPT || signal == cock::abi::process::SIGNAL_TERMINATE ||
		   signal == cock::abi::process::SIGNAL_KILL;
}

bool mayAffect(uint32_t caller, const cock::pm::Row &target) {
	if (cock::pm::callerOf(caller) == cock::pm::Caller::ROOT) return true;
	cock::pm::Row *own = cock::pm::table.findKernel(caller);
	return own != nullptr && own->state != State::EXITED && own->state != State::ZOMBIE &&
		   cock::pm::table.isDescendant(target.pid, own->pid);
}

} // namespace

namespace cock::pm {

Status signalProcess(uint32_t caller, uint32_t target, uint32_t signal) {
	if (!knownSignal(signal)) return Status::BAD_REQUEST;

	Row *row = table.find(target);
	if (row == nullptr || row->state == State::EXITED || row->state == State::ZOMBIE) return Status::NOT_FOUND;
	if (!mayAffect(caller, *row)) return Status::DENIED;
	return cock::user::proc_signal(row->kernelId, signal) ? Status::OK : Status::FAILED;
}

} // namespace cock::pm
