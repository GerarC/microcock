#include <cock/abi/capability.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_inherit.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::security {

using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::security::capability_tree;
using core::security::Capability;
using core::security::CapabilityNodeId;
using core::security::CapabilityType;
using core::security::CAPABILITY_FLAG_BIND;
using core::security::CAPABILITY_FLAG_GRANT;
using core::security::CAPABILITY_FLAG_MINT;
using core::security::NO_CAPABILITY_NODE;
using core::task::Process;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadState;

namespace {

constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr uint64_t FIRST_LABEL = 0;
constexpr uint64_t LABEL_COUNT = 0x100;
constexpr uint64_t SERVICE_LABEL = 7;
constexpr uint64_t FIRST_ENDPOINT = 0x20;
constexpr uint64_t SINGLE = 1;
constexpr uint32_t SERVER_BADGE = 0x77;
constexpr uint64_t PORT_BASE = 0x2F8;
constexpr uint64_t PORT_COUNT = 8;
constexpr size_t FILLED_ENTRIES = core::security::CapabilitySet::MAX_CAPABILITIES;

struct Party {
	Process *process = nullptr;
	Thread *thread = nullptr;

	uint32_t id() const { return process->getId(); }
};

Party makeParty() {
	Party party;
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	party.process = Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	party.thread = new Thread(*party.process, VirtAddr(USER_ENTRY_POINT), ThreadPriority::NORMAL);
	return party;
}

void retire(Party &party) {
	party.thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(party.thread);
	TaskManager::buryDeadThreads();
}

Capability give(Party &party, CapabilityType type, uint32_t flags, uint64_t first, uint64_t count,
				uint32_t badge = 0) {
	Capability capability;
	capability.type = type;
	capability.flags = flags;
	capability.first = first;
	capability.count = count;
	capability.badge = badge;
	capability.node = capability_tree.createRoot(party.id(), type, first, count);
	party.process->getCapabilities().add(capability);
	return capability;
}

bool holds(const Party &party, CapabilityType type, uint64_t first) {
	return party.process->getCapabilities().covers(type, first, SINGLE);
}

void checkIpcAndHandlesAreCopied() {
	Party parent = makeParty();
	Party child = makeParty();
	Capability labels = give(parent, CapabilityType::IPC, CAPABILITY_FLAG_GRANT, FIRST_LABEL, LABEL_COUNT);
	Capability endpoint = give(parent, CapabilityType::ENDPOINT, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT,
							   FIRST_ENDPOINT, SINGLE, SERVER_BADGE);

	EXPECT_TRUE(core::security::can_inherit_capabilities(*parent.process));
	EXPECT_TRUE(core::security::inherit_capabilities(*parent.process, *child.process));

	EXPECT_TRUE(holds(child, CapabilityType::IPC, SERVICE_LABEL));
	EXPECT_FALSE(child.process->getCapabilities().mayGrant(CapabilityType::IPC, SERVICE_LABEL, SINGLE));

	const Capability *copy = child.process->getCapabilities().endpointHandle(endpoint.node);
	EXPECT_NOT_NULL(copy);
	if (copy != nullptr) {
		EXPECT_EQ(copy->badge, SERVER_BADGE);
		EXPECT_EQ(copy->flags, static_cast<uint32_t>(0));
		EXPECT_NEQ(copy->node, endpoint.node);
		const core::security::CapabilityNode *record = capability_tree.find(copy->node);
		EXPECT_NOT_NULL(record);
		if (record != nullptr) {
			EXPECT_EQ(record->parent, endpoint.node);
			EXPECT_EQ(record->holder, child.id());
		}
	}

	const core::security::CapabilityNode *source = capability_tree.find(labels.node);
	EXPECT_NOT_NULL(source);

	retire(child);
	retire(parent);
}

void checkTheIdentityBadgeBecomesTheChilds() {
	Party parent = makeParty();
	Party child = makeParty();
	Capability identity = give(parent, CapabilityType::ENDPOINT, CAPABILITY_FLAG_GRANT, FIRST_ENDPOINT, SINGLE,
							   parent.id());

	EXPECT_TRUE(core::security::inherit_capabilities(*parent.process, *child.process));
	const Capability *copy = child.process->getCapabilities().endpointHandle(identity.node);
	EXPECT_NOT_NULL(copy);
	if (copy != nullptr) EXPECT_EQ(copy->badge, child.id());

	const Capability *original = parent.process->getCapabilities().endpointHandle(identity.node);
	EXPECT_NOT_NULL(original);
	if (original != nullptr) EXPECT_EQ(original->badge, parent.id());

	retire(child);
	retire(parent);
}

void checkHardwareAndOwnerEntriesStayWithTheParent() {
	Party parent = makeParty();
	Party child = makeParty();
	give(parent, CapabilityType::IO_PORTS, CAPABILITY_FLAG_GRANT, PORT_BASE, PORT_COUNT);
	give(parent, CapabilityType::SPAWN, CAPABILITY_FLAG_GRANT, abi::SPAWN_FIRST, abi::SPAWN_COUNT);
	give(parent, CapabilityType::ENDPOINT, CAPABILITY_FLAG_BIND | CAPABILITY_FLAG_GRANT, FIRST_ENDPOINT, SINGLE);

	EXPECT_TRUE(core::security::inherit_capabilities(*parent.process, *child.process));
	EXPECT_FALSE(child.process->getCapabilities().covers(CapabilityType::IO_PORTS, PORT_BASE, SINGLE));
	EXPECT_FALSE(child.process->getCapabilities().covers(CapabilityType::SPAWN, abi::SPAWN_FIRST,
														 abi::SPAWN_COUNT));
	EXPECT_FALSE(child.process->getCapabilities().covers(CapabilityType::ENDPOINT, FIRST_ENDPOINT, SINGLE));

	retire(child);
	retire(parent);
}

void checkAnEntryWithNoNodeBlocksTheCopy() {
	Party parent = makeParty();
	Capability orphan;
	orphan.type = CapabilityType::IPC;
	orphan.first = SERVICE_LABEL;
	orphan.count = SINGLE;
	parent.process->getCapabilities().add(orphan);

	EXPECT_FALSE(core::security::can_inherit_capabilities(*parent.process));
	retire(parent);
}

void checkAFullTableLeavesNothingBehind() {
	Party parent = makeParty();
	Party child = makeParty();
	give(parent, CapabilityType::IPC, CAPABILITY_FLAG_GRANT, FIRST_LABEL, LABEL_COUNT);
	for (uint64_t port = 0; port < FILLED_ENTRIES; port++) {
		Capability filler;
		filler.type = CapabilityType::IO_PORTS;
		filler.first = port;
		filler.count = SINGLE;
		EXPECT_TRUE(child.process->getCapabilities().add(filler));
	}
	size_t before = capability_tree.size();

	EXPECT_FALSE(core::security::inherit_capabilities(*parent.process, *child.process));
	EXPECT_EQ(capability_tree.size(), before);
	EXPECT_FALSE(holds(child, CapabilityType::IPC, SERVICE_LABEL));

	retire(child);
	retire(parent);
}

void checkExitOfTheParentKeepsTheChildCopy() {
	Party parent = makeParty();
	Party child = makeParty();
	give(parent, CapabilityType::IPC, CAPABILITY_FLAG_GRANT, FIRST_LABEL, LABEL_COUNT);
	EXPECT_TRUE(core::security::inherit_capabilities(*parent.process, *child.process));

	retire(parent);
	EXPECT_TRUE(holds(child, CapabilityType::IPC, SERVICE_LABEL));
	retire(child);
}

} // namespace

void test_capability_inherit() {
	TestRunner::begin_suite("Capability inheritance");
	checkIpcAndHandlesAreCopied();
	checkTheIdentityBadgeBecomesTheChilds();
	checkHardwareAndOwnerEntriesStayWithTheParent();
	checkAnEntryWithNoNodeBlocksTheCopy();
	checkAFullTableLeavesNothingBehind();
	checkExitOfTheParentKeepsTheChildCopy();
	TestRunner::end_suite();
}

} // namespace cock::test::security
