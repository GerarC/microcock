#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/ipc_access.hpp>
#include <cock/core/syscall/proc/spawn_rights.hpp>
#include <string.h>

namespace cock::core::syscall {

using memory::VirtAddr;
using memory::VirtualMemoryManager;
using security::capability_tree;
using security::Capability;
using security::CapabilityType;
using security::SINGLE_LABEL;
using task::Process;

bool spawn_rights_valid(const Process &creator, const abi::service::SpawnRights &rights) {
	if (rights.label == abi::service::NO_SERVICE ||
		rights.ipcCount > abi::service::MAX_SPAWN_IPC ||
		!capability_tree.hasRoom(rights.ipcCount))
		return false;
	if (!creator.getCapabilities().mayGrant(CapabilityType::IPC, rights.label, SINGLE_LABEL))
		return false;

	for (uint32_t index = 0; index < rights.ipcCount; index++) {
		const Capability *source = creator.getCapabilities().grantable(
			CapabilityType::IPC, rights.ipc[index], SINGLE_LABEL);
		if (source == nullptr || capability_tree.find(source->node) == nullptr) return false;
	}
	return true;
}

bool read_spawn_rights(const Process &creator, VirtAddr limit, uintptr_t rights_ptr,
					   abi::service::SpawnRights &rights) {
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(rights_ptr), sizeof(rights), false,
												limit))
		return false;
	memcpy(&rights, reinterpret_cast<const void *>(rights_ptr), sizeof(rights));
	return spawn_rights_valid(creator, rights);
}

void give_spawn_rights(const Process &creator, Process &process,
					   const abi::service::SpawnRights &rights) {
	process.setServiceId(rights.label);
	for (uint32_t index = 0; index < rights.ipcCount; index++) {
		const Capability *source = creator.getCapabilities().grantable(
			CapabilityType::IPC, rights.ipc[index], SINGLE_LABEL);
		Capability granted;
		granted.type = CapabilityType::IPC;
		granted.first = rights.ipc[index];
		granted.count = SINGLE_LABEL;
		granted.node = capability_tree.derive(source->node, process.getId(),
											  CapabilityType::IPC, granted.first, granted.count);
		process.getCapabilities().add(granted);
	}
}

} // namespace cock::core::syscall
