#include <cock/abi/capability.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_transfer.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/revocation.hpp>
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
using core::security::CAPABILITY_FLAG_GRANT;
using core::security::delegate_capability;
using core::security::NO_CAPABILITY_NODE;
using core::security::revoke_capability;
using core::security::withdraw_capability;
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
constexpr uint64_t OTHER_LABEL = 9;
constexpr uint64_t SINGLE = 1;
constexpr uint64_t EMPTY_RANGE = 0;
constexpr uint32_t MASK_WITH_NO_BIT_SET = 0;
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

CapabilityNodeId giveRoot(Party &party, CapabilityType type, uint64_t first, uint64_t count) {
	Capability capability;
	capability.type = type;
	capability.flags = CAPABILITY_FLAG_GRANT;
	capability.first = first;
	capability.count = count;
	capability.node = capability_tree.createRoot(party.id(), type, first, count);
	party.process->getCapabilities().add(capability);
	return capability.node;
}

bool holds(const Party &party, CapabilityType type, uint64_t first) {
	return party.process->getCapabilities().covers(type, first, SINGLE);
}

bool mayPassOn(const Party &party, CapabilityType type, uint64_t first) {
	return party.process->getCapabilities().mayGrant(type, first, SINGLE);
}

void checkNothingIsAcceptedByDefault() {
	Party party = makeParty();
	for (uint32_t type = static_cast<uint32_t>(abi::FIRST_CAPABILITY_TYPE);
		 type <= static_cast<uint32_t>(abi::HIGHEST_CAPABILITY_TYPE); type++)
		EXPECT_FALSE(party.process->acceptsCapability(static_cast<CapabilityType>(type)));
	retire(party);
}

void checkAcceptMaskSelectsTypes() {
	Party party = makeParty();
	party.process->acceptCapabilities(abi::capabilityBit(CapabilityType::IPC));
	EXPECT_TRUE(party.process->acceptsCapability(CapabilityType::IPC));
	EXPECT_FALSE(party.process->acceptsCapability(CapabilityType::IO_PORTS));
	EXPECT_FALSE(party.process->acceptsCapability(CapabilityType::SPAWN));

	party.process->acceptCapabilities(abi::ALL_CAPABILITY_TYPES);
	EXPECT_TRUE(party.process->acceptsCapability(CapabilityType::IO_PORTS));
	EXPECT_TRUE(party.process->acceptsCapability(CapabilityType::SPAWN));

	party.process->acceptCapabilities(MASK_WITH_NO_BIT_SET);
	EXPECT_FALSE(party.process->acceptsCapability(CapabilityType::IPC));
	retire(party);
}

void checkAllTypesMaskCoversExactlyTheTypes() {
	uint32_t expected = MASK_WITH_NO_BIT_SET;
	for (uint32_t type = static_cast<uint32_t>(abi::FIRST_CAPABILITY_TYPE);
		 type <= static_cast<uint32_t>(abi::HIGHEST_CAPABILITY_TYPE); type++)
		expected |= abi::capabilityBit(static_cast<CapabilityType>(type));
	EXPECT_EQ(abi::ALL_CAPABILITY_TYPES, expected);
	EXPECT_EQ(abi::ALL_CAPABILITY_TYPES & abi::capabilityBit(CapabilityType::NONE), MASK_WITH_NO_BIT_SET);
}

void checkDelegationRecordsTheCopyInTheTree() {
	Party server = makeParty();
	Party client = makeParty();
	CapabilityNodeId top = giveRoot(server, CapabilityType::IPC, FIRST_LABEL, LABEL_COUNT);

	CapabilityNodeId node = delegate_capability(*server.process, *client.process, CapabilityType::IPC,
												SERVICE_LABEL, SINGLE, false);
	EXPECT_NEQ(node, NO_CAPABILITY_NODE);
	EXPECT_TRUE(holds(client, CapabilityType::IPC, SERVICE_LABEL));
	EXPECT_FALSE(holds(client, CapabilityType::IPC, OTHER_LABEL));
	EXPECT_FALSE(mayPassOn(client, CapabilityType::IPC, SERVICE_LABEL));

	const core::security::CapabilityNode *record = capability_tree.find(node);
	EXPECT_NOT_NULL(record);
	if (record != nullptr) {
		EXPECT_EQ(record->parent, top);
		EXPECT_EQ(record->holder, client.id());
	}

	retire(client);
	retire(server);
}

void checkDelegateFlagLetsTheReceiverPassOn() {
	Party server = makeParty();
	Party client = makeParty();
	giveRoot(server, CapabilityType::IPC, FIRST_LABEL, LABEL_COUNT);

	EXPECT_NEQ(delegate_capability(*server.process, *client.process, CapabilityType::IPC,
								   SERVICE_LABEL, SINGLE, true),
			   NO_CAPABILITY_NODE);
	EXPECT_TRUE(mayPassOn(client, CapabilityType::IPC, SERVICE_LABEL));

	retire(client);
	retire(server);
}

void checkGiverNeedsAGrantableEntry() {
	Party server = makeParty();
	Party client = makeParty();
	Party third = makeParty();
	giveRoot(server, CapabilityType::IPC, FIRST_LABEL, LABEL_COUNT);
	EXPECT_NEQ(delegate_capability(*server.process, *client.process, CapabilityType::IPC,
								   SERVICE_LABEL, SINGLE, false),
			   NO_CAPABILITY_NODE);

	EXPECT_EQ(delegate_capability(*client.process, *third.process, CapabilityType::IPC,
								  SERVICE_LABEL, SINGLE, false),
			  NO_CAPABILITY_NODE);
	EXPECT_FALSE(holds(third, CapabilityType::IPC, SERVICE_LABEL));

	EXPECT_EQ(delegate_capability(*server.process, *third.process, CapabilityType::IO_PORTS,
								  SERVICE_LABEL, SINGLE, false),
			  NO_CAPABILITY_NODE);
	EXPECT_EQ(delegate_capability(*server.process, *third.process, CapabilityType::IPC,
								  LABEL_COUNT, SINGLE, false),
			  NO_CAPABILITY_NODE);
	EXPECT_EQ(delegate_capability(*server.process, *third.process, CapabilityType::IPC,
								  SERVICE_LABEL, EMPTY_RANGE, false),
			  NO_CAPABILITY_NODE);

	retire(third);
	retire(client);
	retire(server);
}

void checkChainOfTransfersIsRevocableFromTheTop() {
	Party server = makeParty();
	Party client = makeParty();
	Party third = makeParty();
	giveRoot(server, CapabilityType::IPC, FIRST_LABEL, LABEL_COUNT);

	CapabilityNodeId first = delegate_capability(*server.process, *client.process, CapabilityType::IPC,
												 SERVICE_LABEL, SINGLE, true);
	CapabilityNodeId second = delegate_capability(*client.process, *third.process, CapabilityType::IPC,
												  SERVICE_LABEL, SINGLE, false);
	EXPECT_NEQ(first, NO_CAPABILITY_NODE);
	EXPECT_NEQ(second, NO_CAPABILITY_NODE);
	EXPECT_TRUE(holds(third, CapabilityType::IPC, SERVICE_LABEL));

	EXPECT_TRUE(revoke_capability(*server.process, first));
	EXPECT_FALSE(holds(client, CapabilityType::IPC, SERVICE_LABEL));
	EXPECT_FALSE(holds(third, CapabilityType::IPC, SERVICE_LABEL));
	EXPECT_TRUE(capability_tree.find(second) == nullptr);

	retire(third);
	retire(client);
	retire(server);
}

void checkFullTableLeavesNothingBehind() {
	Party server = makeParty();
	Party client = makeParty();
	giveRoot(server, CapabilityType::IPC, FIRST_LABEL, LABEL_COUNT);
	size_t before = capability_tree.size();

	for (uint64_t label = FIRST_LABEL; label < FILLED_ENTRIES; label++) {
		Capability filler;
		filler.type = CapabilityType::IO_PORTS;
		filler.first = label;
		filler.count = SINGLE;
		EXPECT_TRUE(client.process->getCapabilities().add(filler));
	}

	EXPECT_EQ(delegate_capability(*server.process, *client.process, CapabilityType::IPC,
								  SERVICE_LABEL, SINGLE, false),
			  NO_CAPABILITY_NODE);
	EXPECT_EQ(capability_tree.size(), before);

	retire(client);
	retire(server);
}

void checkWithdrawUndoesADelegation() {
	Party server = makeParty();
	Party client = makeParty();
	giveRoot(server, CapabilityType::IPC, FIRST_LABEL, LABEL_COUNT);
	size_t before = capability_tree.size();

	CapabilityNodeId node = delegate_capability(*server.process, *client.process, CapabilityType::IPC,
												SERVICE_LABEL, SINGLE, false);
	EXPECT_NEQ(node, NO_CAPABILITY_NODE);
	EXPECT_TRUE(withdraw_capability(*client.process, node));
	EXPECT_FALSE(holds(client, CapabilityType::IPC, SERVICE_LABEL));
	EXPECT_EQ(capability_tree.size(), before);
	EXPECT_FALSE(withdraw_capability(*client.process, node));

	retire(client);
	retire(server);
}

} // namespace

void test_capability_transfer() {
	TestRunner::begin_suite("Capability transfer");
	checkNothingIsAcceptedByDefault();
	checkAcceptMaskSelectsTypes();
	checkAllTypesMaskCoversExactlyTheTypes();
	checkDelegationRecordsTheCopyInTheTree();
	checkDelegateFlagLetsTheReceiverPassOn();
	checkGiverNeedsAGrantableEntry();
	checkChainOfTransfersIsRevocableFromTheTop();
	checkFullTableLeavesNothingBehind();
	checkWithdrawUndoesADelegation();
	TestRunner::end_suite();
}

} // namespace cock::test::security
