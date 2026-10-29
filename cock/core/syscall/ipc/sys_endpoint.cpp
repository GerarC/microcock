#include <cock/abi/endpoint.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/endpoint_table.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/reply_table.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::security::Capability;
using core::security::capability_tree;
using core::security::CapabilityNodeId;
using core::security::CapabilityType;
using core::security::NO_CAPABILITY_NODE;
using core::task::endpoint_table;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

uintptr_t sys_ep_create(uintptr_t handle_ptr, uintptr_t, uintptr_t, uintptr_t,
						uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	task::Process &process = *current->getProcess();
	if (!process.getCapabilities().holdsGrantAuthority()) return FAILURE;
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(handle_ptr), sizeof(uint32_t), true,
												current->getUserSpaceLimit()))
		return FAILURE;

	uint32_t id = endpoint_table.create();
	if (id == abi::endpoint::NO_ENDPOINT) return FAILURE;

	uint32_t handle = NO_CAPABILITY_NODE;
	if (!process.getCapabilities().hasFlag(CapabilityType::ENDPOINT, id,
										   abi::endpoint::SINGLE_ENDPOINT,
										   security::CAPABILITY_FLAG_BIND)) {
		CapabilityNodeId node = capability_tree.createRoot(process.getId(), CapabilityType::ENDPOINT,
														   id, abi::endpoint::SINGLE_ENDPOINT);
		Capability owner;
		owner.type = CapabilityType::ENDPOINT;
		owner.flags = security::CAPABILITY_FLAG_GRANT | security::CAPABILITY_FLAG_MINT |
					  security::CAPABILITY_FLAG_BIND;
		owner.first = id;
		owner.count = abi::endpoint::SINGLE_ENDPOINT;
		owner.node = node;
		if (node == NO_CAPABILITY_NODE || !process.getCapabilities().add(owner)) {
			capability_tree.detach(node);
			endpoint_table.destroy(id);
			return FAILURE;
		}
		handle = node;
	}

	memcpy(reinterpret_cast<void *>(handle_ptr), &handle, sizeof(handle));
	return id;
}

uintptr_t sys_ep_bind(uintptr_t endpoint, uintptr_t pid, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || endpoint > UINT32_MAX) return FAILURE;
	if (!current->getProcess()->getCapabilities().hasFlag(
			CapabilityType::ENDPOINT, endpoint, abi::endpoint::SINGLE_ENDPOINT,
			security::CAPABILITY_FLAG_BIND))
		return FAILURE;

	Thread *target = TaskManager::getThreadByPID(static_cast<uint32_t>(pid));
	if (!target || target->getProcess()->isKernel() || !target->isMainThread()) return FAILURE;

	if (!endpoint_table.bind(static_cast<uint32_t>(endpoint), target->getProcess()->getId(),
							 core::task::closeTokenOfMessage))
		return FAILURE;

	core::task::IPC::announce(*target);
	return SUCCESS;
}

} // namespace cock::core::syscall
