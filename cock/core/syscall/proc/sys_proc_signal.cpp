#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/abi/process.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/ipc/message.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using ipc::Message;
using ipc::MessageType;
using security::CapabilityType;
using task::IPC;
using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using task::ThreadState;
using utils::Logger;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

uintptr_t deliver_signal(Thread &target, uint32_t signal) {
	Message message = {};
	message.type = MessageType::SIGNAL;
	message.badge = abi::endpoint::NO_BADGE;
	message.reply = abi::endpoint::NO_REPLY;
	message.payload.args.arg0 = signal;
	return IPC::deliver(target.getId(), message) ? SUCCESS : FAILURE;
}

} // namespace

uintptr_t sys_proc_signal(uintptr_t pid, uintptr_t signal, uintptr_t, uintptr_t, uintptr_t,
						  uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (current == nullptr || pid > UINT32_MAX) return FAILURE;
	if (signal != abi::process::SIGNAL_KILL && signal != abi::process::SIGNAL_TERMINATE &&
		signal != abi::process::SIGNAL_INTERRUPT && signal != abi::process::SIGNAL_STOP &&
		signal != abi::process::SIGNAL_CONTINUE)
		return FAILURE;

	Process &sender = *current->getProcess();
	if (!sender.getCapabilities().covers(CapabilityType::SPAWN, abi::SPAWN_FIRST, abi::SPAWN_COUNT)) {
		Logger::trace("sys_proc_signal: PID %d holds no spawn capability", sender.getId());
		return FAILURE;
	}

	Thread *target = TaskManager::getThreadByPID(static_cast<uint32_t>(pid));
	if (target == nullptr || !target->isMainThread() || target->getState() == ThreadState::DEAD)
		return FAILURE;

	Process &process = *target->getProcess();
	if (process.isKernel() || &process == &sender) return FAILURE;

	if (signal == abi::process::SIGNAL_STOP) {
		TaskManager::stopProcess(process);
		return SUCCESS;
	}
	if (signal == abi::process::SIGNAL_CONTINUE) {
		TaskManager::continueProcess(process);
		return SUCCESS;
	}
	if (signal != abi::process::SIGNAL_KILL) return deliver_signal(*target, static_cast<uint32_t>(signal));

	process.setExitStatus(abi::process::KILLED_STATUS);
	TaskManager::terminateProcess(process);
	return SUCCESS;
}

} // namespace cock::core::syscall
