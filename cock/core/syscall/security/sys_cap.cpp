#include <cock/core/security/capability.hpp>
#include <cock/core/security/capability_transfer.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/revocation.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using security::Capability;
using security::capability_tree;
using security::HIGHEST_CAPABILITY_NODE;
using security::CapabilityNodeId;
using security::CapabilityType;
using security::NO_CAPABILITY_NODE;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using utils::Logger;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;
constexpr uintptr_t HIGHEST_CAPABILITY_TYPE =
	static_cast<uintptr_t>(abi::HIGHEST_CAPABILITY_TYPE);

uintptr_t sys_cap_grant(uintptr_t target_pid, uintptr_t type, uintptr_t first,
						uintptr_t count, uintptr_t flags, uintptr_t badge) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	if (type == 0 || type > HIGHEST_CAPABILITY_TYPE) return FAILURE;
	if (flags & ~static_cast<uintptr_t>(abi::ALL_GRANT_FLAGS)) return FAILURE;

	Thread *target =
		TaskManager::getThreadByPID(static_cast<uint32_t>(target_pid));
	if (!target || target->getProcess()->isKernel()) return FAILURE;
	if (target->getProcess()->getId() != target_pid) return FAILURE;

	CapabilityNodeId node = security::delegate_capability(
		*current->getProcess(), *target->getProcess(),
		static_cast<CapabilityType>(type), first, count,
		(flags & abi::GRANT_DELEGATE) != 0, (flags & abi::GRANT_MINT) != 0,
		static_cast<uint32_t>(badge));
	if (node == NO_CAPABILITY_NODE) {
		Logger::trace("sys_cap_grant: PID %d may not grant type %u [0x%x, +0x%x)",
					 current->getProcess()->getId(),
					 static_cast<unsigned>(type), static_cast<unsigned>(first),
					 static_cast<unsigned>(count));
		return FAILURE;
	}
	return node;
}

uintptr_t sys_cap_accept(uintptr_t mask, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	if (mask & ~static_cast<uintptr_t>(abi::ALL_CAPABILITY_TYPES)) return FAILURE;

	current->getProcess()->acceptCapabilities(static_cast<uint32_t>(mask));
	return SUCCESS;
}

uintptr_t sys_cap_revoke(uintptr_t node, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	if (node == NO_CAPABILITY_NODE || node > HIGHEST_CAPABILITY_NODE) return FAILURE;

	return security::revoke_capability(*current->getProcess(),
									   static_cast<CapabilityNodeId>(node))
			   ? SUCCESS
			   : FAILURE;
}

} // namespace cock::core::syscall
