#include <cock/abi/service.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using utils::Logger;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

} // namespace

uintptr_t sys_service_set(uintptr_t target_pid, uintptr_t label, uintptr_t,
						  uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !current->getProcess()->getCapabilities().holdsGrantAuthority())
		return FAILURE;
	if (target_pid > UINT32_MAX || label > UINT32_MAX) return FAILURE;

	Thread *target = TaskManager::getThreadByPID(static_cast<uint32_t>(target_pid));
	if (!target || target->getProcess()->isKernel() ||
		target->getProcess()->getId() != target_pid)
		return FAILURE;

	if (!target->getProcess()->setServiceId(static_cast<uint32_t>(label))) {
		Logger::trace("sys_service_set: PID %d cannot label PID %u", current->getId(),
					 static_cast<unsigned>(target_pid));
		return FAILURE;
	}
	return SUCCESS;
}

} // namespace cock::core::syscall
