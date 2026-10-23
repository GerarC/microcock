#include <cock/abi/service.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/ipc_access.hpp>
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
using core::security::mayTalk;
using core::security::NO_CAPABILITY_NODE;
using core::security::revoke_capability;
using core::task::Process;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadState;

namespace {

constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr uint32_t CLIENT_LABEL = 20;
constexpr uint32_t SERVER_LABEL = 21;
constexpr uint32_t OTHER_LABEL = 22;
constexpr uint64_t SINGLE = 1;
constexpr uint64_t LABEL_RANGE = 4;
constexpr uint32_t NOT_COVERED_LABEL = 40;
constexpr size_t ONE_NODE = 1;

struct Party {
	Process *process = nullptr;
	Thread *thread = nullptr;
};

Party makeParty(uint32_t label) {
	Party party;
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	party.process =
		Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	party.thread = new Thread(*party.process, VirtAddr(USER_ENTRY_POINT),
							  ThreadPriority::NORMAL);
	if (label != abi::service::NO_SERVICE) party.process->setServiceId(label);
	return party;
}

void retire(Party &party) {
	party.thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(party.thread);
	TaskManager::buryDeadThreads();
}

CapabilityNodeId giveRoot(Party &party, uint64_t first, uint64_t count) {
	Capability capability;
	capability.type = CapabilityType::IPC;
	capability.flags = CAPABILITY_FLAG_GRANT;
	capability.first = first;
	capability.count = count;
	capability.node = capability_tree.createRoot(party.process->getId(), CapabilityType::IPC,
												 first, count);
	party.process->getCapabilities().add(capability);
	return capability.node;
}

CapabilityNodeId giveLabel(Party &to, CapabilityNodeId parent, uint64_t first, uint64_t count) {
	Capability capability;
	capability.type = CapabilityType::IPC;
	capability.first = first;
	capability.count = count;
	capability.node =
		capability_tree.derive(parent, to.process->getId(), CapabilityType::IPC, first, count);
	to.process->getCapabilities().add(capability);
	return capability.node;
}

bool talk(const Party &from, const Party &to) {
	return mayTalk(*from.process, *to.process);
}

void checkSameProcessAndNoCapability() {
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);

	EXPECT_TRUE(talk(client, client));
	EXPECT_FALSE(talk(client, server));
	EXPECT_FALSE(talk(server, client));

	retire(server);
	retire(client);
}

void checkSenderCapabilityWorksBothWays() {
	Party root = makeParty(abi::service::ROOT_SERVICE);
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	CapabilityNodeId top = giveRoot(root, 0, abi::service::SERVICE_LABEL_COUNT);
	giveLabel(client, top, SERVER_LABEL, SINGLE);

	EXPECT_TRUE(talk(client, server));
	EXPECT_TRUE(talk(server, client));

	retire(server);
	retire(client);
	retire(root);
}

void checkReceiverCapabilityWorksBothWays() {
	Party root = makeParty(abi::service::ROOT_SERVICE);
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	CapabilityNodeId top = giveRoot(root, 0, abi::service::SERVICE_LABEL_COUNT);
	giveLabel(server, top, CLIENT_LABEL, SINGLE);

	EXPECT_TRUE(talk(client, server));
	EXPECT_TRUE(talk(server, client));

	retire(server);
	retire(client);
	retire(root);
}

void checkRangeAndLabels() {
	Party root = makeParty(abi::service::ROOT_SERVICE);
	Party client = makeParty(CLIENT_LABEL);
	Party inRange = makeParty(OTHER_LABEL);
	Party outOfRange = makeParty(NOT_COVERED_LABEL);
	Party unlabeled = makeParty(abi::service::NO_SERVICE);
	CapabilityNodeId top = giveRoot(root, 0, abi::service::SERVICE_LABEL_COUNT);
	giveLabel(client, top, SERVER_LABEL, LABEL_RANGE);

	EXPECT_TRUE(talk(client, inRange));
	EXPECT_FALSE(talk(client, outOfRange));
	EXPECT_FALSE(talk(outOfRange, client));
	EXPECT_FALSE(talk(client, unlabeled));
	EXPECT_FALSE(talk(unlabeled, client));
	EXPECT_FALSE(talk(inRange, outOfRange));

	retire(unlabeled);
	retire(outOfRange);
	retire(inRange);
	retire(client);
	retire(root);
}

void checkRootReachesEveryone() {
	Party root = makeParty(abi::service::ROOT_SERVICE);
	Party client = makeParty(CLIENT_LABEL);
	Party unlabeled = makeParty(abi::service::NO_SERVICE);
	giveRoot(root, 0, abi::service::SERVICE_LABEL_COUNT);

	EXPECT_TRUE(talk(root, client));
	EXPECT_TRUE(talk(client, root));
	EXPECT_TRUE(talk(root, unlabeled));
	EXPECT_TRUE(talk(unlabeled, root));

	retire(unlabeled);
	retire(client);
	retire(root);
}

void checkRevocationCutsBothDirections() {
	Party root = makeParty(abi::service::ROOT_SERVICE);
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	CapabilityNodeId top = giveRoot(root, 0, abi::service::SERVICE_LABEL_COUNT);
	CapabilityNodeId node = giveLabel(client, top, SERVER_LABEL, SINGLE);
	EXPECT_TRUE(talk(client, server));

	EXPECT_TRUE(revoke_capability(*root.process, node));
	EXPECT_FALSE(talk(client, server));
	EXPECT_FALSE(talk(server, client));

	retire(server);
	retire(client);
	retire(root);
}

void checkLabelIsSetOnce() {
	Party party = makeParty(abi::service::NO_SERVICE);
	EXPECT_EQ(abi::service::NO_SERVICE, party.process->getServiceId());
	EXPECT_FALSE(party.process->setServiceId(abi::service::NO_SERVICE));
	EXPECT_TRUE(party.process->setServiceId(CLIENT_LABEL));
	EXPECT_EQ(CLIENT_LABEL, party.process->getServiceId());
	EXPECT_FALSE(party.process->setServiceId(SERVER_LABEL));
	EXPECT_EQ(CLIENT_LABEL, party.process->getServiceId());
	retire(party);
}

void checkNodesLeaveWithTheirHolders() {
	Party root = makeParty(abi::service::ROOT_SERVICE);
	Party client = makeParty(CLIENT_LABEL);
	size_t before = capability_tree.size();
	CapabilityNodeId top = giveRoot(root, 0, abi::service::SERVICE_LABEL_COUNT);
	giveLabel(client, top, SERVER_LABEL, SINGLE);
	EXPECT_EQ(before + ONE_NODE + ONE_NODE, capability_tree.size());

	retire(client);
	EXPECT_EQ(before + ONE_NODE, capability_tree.size());
	retire(root);
	EXPECT_EQ(before, capability_tree.size());
}

} // namespace

void test_ipc_access() {
	TestRunner::begin_suite("IPC access");
	checkSameProcessAndNoCapability();
	checkSenderCapabilityWorksBothWays();
	checkReceiverCapabilityWorksBothWays();
	checkRangeAndLabels();
	checkRootReachesEveryone();
	checkRevocationCutsBothDirections();
	checkLabelIsSetOnce();
	checkNodesLeaveWithTheirHolders();
	TestRunner::end_suite();
}

} // namespace cock::test::security
