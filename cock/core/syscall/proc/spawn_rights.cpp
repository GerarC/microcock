#include <cock/core/memory/vmm.hpp>
#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/core/security/capability_transfer.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/syscall/proc/spawn_rights.hpp>
#include <string.h>

namespace cock::core::syscall {

namespace {

constexpr size_t NO_HANDLES = 0;
constexpr size_t SINGLE_HANDLE = 1;
constexpr uint64_t SINGLE_LABEL = 1;

} // namespace

using memory::VirtAddr;
using memory::VirtualMemoryManager;
using security::capability_tree;
using security::Capability;
using security::CAPABILITY_FLAG_GRANT;
using security::CAPABILITY_FLAG_MINT;
using security::CapabilityType;
using security::NO_CAPABILITY_NODE;
using task::Process;

bool spawn_rights_valid(const Process &creator, const abi::service::SpawnRights &rights) {
	bool has_handle = rights.initialEndpoint != abi::endpoint::NO_ENDPOINT;
	if (rights.label == abi::service::NO_SERVICE ||
		rights.ipcCount > abi::service::MAX_SPAWN_IPC ||
		(rights.initialFlags & ~abi::ALL_GRANT_FLAGS) != 0 ||
		!capability_tree.hasRoom(rights.ipcCount + (has_handle ? SINGLE_HANDLE : NO_HANDLES)))
		return false;
	if (!creator.getCapabilities().mayGrant(CapabilityType::IPC, rights.label, SINGLE_LABEL))
		return false;
	if (has_handle &&
		!creator.getCapabilities().hasFlag(CapabilityType::ENDPOINT, rights.initialEndpoint,
										   abi::endpoint::SINGLE_ENDPOINT,
										   CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT))
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

uint32_t give_spawn_rights(const Process &creator, Process &process,
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

	if (rights.initialEndpoint == abi::endpoint::NO_ENDPOINT) return NO_CAPABILITY_NODE;
	return security::delegate_capability(
		creator, process, CapabilityType::ENDPOINT, rights.initialEndpoint,
		abi::endpoint::SINGLE_ENDPOINT, (rights.initialFlags & abi::GRANT_DELEGATE) != 0,
		(rights.initialFlags & abi::GRANT_MINT) != 0, process.getId());
}

} // namespace cock::core::syscall
