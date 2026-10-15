#include <cock/core/security/capability.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using security::Capability;
using security::CapabilityType;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using utils::Logger;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;
constexpr uintptr_t HIGHEST_CAPABILITY_TYPE =
	static_cast<uintptr_t>(CapabilityType::PHYSICAL_MEMORY);

uintptr_t sys_cap_grant(uintptr_t target_pid, uintptr_t type, uintptr_t first,
						uintptr_t count, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	if (type == 0 || type > HIGHEST_CAPABILITY_TYPE) return FAILURE;

	CapabilityType capability_type = static_cast<CapabilityType>(type);
	if (!current->getProcess()->getCapabilities().mayGrant(capability_type,
														   first, count)) {
		Logger::trace("sys_cap_grant: PID %d may not grant type %u [0x%x, +0x%x)",
					 current->getProcess()->getId(),
					 static_cast<unsigned>(type), static_cast<unsigned>(first),
					 static_cast<unsigned>(count));
		return FAILURE;
	}

	Thread *target =
		TaskManager::getThreadByPID(static_cast<uint32_t>(target_pid));
	if (!target || target->getProcess()->isKernel()) return FAILURE;
	if (target->getProcess()->getId() != target_pid) return FAILURE;

	Capability granted;
	granted.type = capability_type;
	granted.first = first;
	granted.count = count;
	return target->getProcess()->getCapabilities().add(granted) ? SUCCESS
																: FAILURE;
}

} // namespace cock::core::syscall
