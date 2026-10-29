#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/abi/service.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/ipc_access.hpp>
#include <cock/core/security/revocation.hpp>
#include <cock/core/syscall/proc/spawn_rights.hpp>
#include <cock/core/task/endpoint_table.hpp>
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
using core::security::CAPABILITY_FLAG_MINT;
using core::security::mayShare;
using core::security::NO_CAPABILITY_NODE;
using core::security::revoke_capability;
using core::syscall::give_spawn_rights;
using core::syscall::spawn_rights_valid;
using core::task::endpoint_table;
using core::task::Process;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadState;

namespace {

constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr uint32_t CLIENT_LABEL = 20;
constexpr uint32_t SERVER_LABEL = 21;
constexpr uint64_t SINGLE = 1;
constexpr size_t ONE_NODE = 1;
constexpr uint32_t NO_FLAGS = 0;
constexpr uint32_t UNKNOWN_FLAG = 0x80;

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

uint32_t endpointOf(const Party &owner) {
	uint32_t id = endpoint_table.create();
	endpoint_table.bind(id, owner.process->getId());
	return id;
}

CapabilityNodeId giveHandle(Party &holder, uint32_t endpoint, uint32_t flags) {
	Capability capability;
	capability.type = CapabilityType::ENDPOINT;
	capability.flags = flags;
	capability.first = endpoint;
	capability.count = abi::endpoint::SINGLE_ENDPOINT;
	capability.node = capability_tree.createRoot(holder.process->getId(), CapabilityType::ENDPOINT,
												 endpoint, abi::endpoint::SINGLE_ENDPOINT);
	holder.process->getCapabilities().add(capability);
	return capability.node;
}

bool share(const Party &from, const Party &to) {
	return mayShare(*from.process, *to.process);
}

void checkSameProcessAndNoHandle() {
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);

	EXPECT_TRUE(share(client, client));
	EXPECT_FALSE(share(client, server));
	EXPECT_FALSE(share(server, client));

	retire(server);
	retire(client);
}

void checkHandleOfTheSenderWorksBothWays() {
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	giveHandle(client, endpointOf(server), NO_FLAGS);

	EXPECT_TRUE(share(client, server));
	EXPECT_TRUE(share(server, client));

	retire(server);
	retire(client);
}

void checkHandleOfTheReceiverWorksBothWays() {
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	giveHandle(server, endpointOf(client), NO_FLAGS);

	EXPECT_TRUE(share(client, server));
	EXPECT_TRUE(share(server, client));

	retire(server);
	retire(client);
}

void checkAnUnboundEndpointOpensNothing() {
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	uint32_t endpoint = endpoint_table.create();
	giveHandle(client, endpoint, NO_FLAGS);
	EXPECT_FALSE(share(client, server));

	endpoint_table.bind(endpoint, server.process->getId());
	EXPECT_TRUE(share(client, server));

	retire(server);
	retire(client);
}

void checkAHandleDoesNotOpenAThirdProcess() {
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	Party bystander = makeParty(abi::service::NO_SERVICE);
	giveHandle(client, endpointOf(server), NO_FLAGS);

	EXPECT_FALSE(share(client, bystander));
	EXPECT_FALSE(share(bystander, client));
	EXPECT_FALSE(share(server, bystander));

	retire(bystander);
	retire(server);
	retire(client);
}

void checkRevocationCutsBothDirections() {
	Party root = makeParty(abi::service::ROOT_SERVICE);
	Party client = makeParty(CLIENT_LABEL);
	Party server = makeParty(SERVER_LABEL);
	uint32_t endpoint = endpointOf(server);
	CapabilityNodeId top = giveHandle(root, endpoint, CAPABILITY_FLAG_GRANT);
	Capability copy;
	copy.type = CapabilityType::ENDPOINT;
	copy.first = endpoint;
	copy.count = abi::endpoint::SINGLE_ENDPOINT;
	copy.node = capability_tree.derive(top, client.process->getId(), CapabilityType::ENDPOINT,
									   endpoint, abi::endpoint::SINGLE_ENDPOINT);
	client.process->getCapabilities().add(copy);
	EXPECT_TRUE(share(client, server));

	EXPECT_TRUE(revoke_capability(*root.process, copy.node));
	EXPECT_FALSE(share(client, server));
	EXPECT_FALSE(share(server, client));

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

abi::service::SpawnRights rightsWithHandle(uint32_t endpoint, uint32_t flags) {
	abi::service::SpawnRights rights = {};
	rights.label = CLIENT_LABEL;
	rights.initialEndpoint = endpoint;
	rights.initialFlags = flags;
	return rights;
}

void checkSpawnGivesAHandleBadgedWithTheNewProcess() {
	Party creator = makeParty(abi::service::ROOT_SERVICE);
	Party child = makeParty(abi::service::NO_SERVICE);
	giveRoot(creator, 0, abi::service::SERVICE_LABEL_COUNT);
	uint32_t endpoint = endpointOf(creator);
	giveHandle(creator, endpoint, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);

	abi::service::SpawnRights rights = rightsWithHandle(endpoint, NO_FLAGS);
	EXPECT_TRUE(spawn_rights_valid(*creator.process, rights));
	CapabilityNodeId node = give_spawn_rights(*creator.process, *child.process, rights);
	EXPECT_NEQ(NO_CAPABILITY_NODE, node);

	const Capability *handle = child.process->getCapabilities().endpointHandle(node);
	EXPECT_TRUE(handle != nullptr);
	if (handle != nullptr) {
		EXPECT_EQ(child.process->getId(), handle->badge);
		EXPECT_EQ(endpoint, static_cast<uint32_t>(handle->first));
		EXPECT_EQ(NO_FLAGS, handle->flags);
	}
	EXPECT_TRUE(share(creator, child));

	retire(child);
	retire(creator);
}

void checkSpawnWithoutAHandleLeavesNone() {
	Party creator = makeParty(abi::service::ROOT_SERVICE);
	Party child = makeParty(abi::service::NO_SERVICE);
	giveRoot(creator, 0, abi::service::SERVICE_LABEL_COUNT);

	abi::service::SpawnRights rights = rightsWithHandle(abi::endpoint::NO_ENDPOINT, NO_FLAGS);
	EXPECT_TRUE(spawn_rights_valid(*creator.process, rights));
	EXPECT_EQ(NO_CAPABILITY_NODE, give_spawn_rights(*creator.process, *child.process, rights));
	EXPECT_FALSE(share(creator, child));

	retire(child);
	retire(creator);
}

void checkSpawnRefusesAHandleTheCreatorMayNotMint() {
	Party creator = makeParty(abi::service::ROOT_SERVICE);
	giveRoot(creator, 0, abi::service::SERVICE_LABEL_COUNT);
	uint32_t held = endpointOf(creator);
	uint32_t not_held = endpoint_table.create();
	uint32_t grant_only = endpoint_table.create();
	giveHandle(creator, held, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	giveHandle(creator, grant_only, CAPABILITY_FLAG_GRANT);

	EXPECT_TRUE(spawn_rights_valid(*creator.process, rightsWithHandle(held, NO_FLAGS)));
	EXPECT_FALSE(spawn_rights_valid(*creator.process, rightsWithHandle(not_held, NO_FLAGS)));
	EXPECT_FALSE(spawn_rights_valid(*creator.process, rightsWithHandle(grant_only, NO_FLAGS)));
	EXPECT_FALSE(spawn_rights_valid(*creator.process, rightsWithHandle(held, UNKNOWN_FLAG)));

	retire(creator);
}

} // namespace

void test_ipc_access() {
	TestRunner::begin_suite("IPC access");
	checkSameProcessAndNoHandle();
	checkHandleOfTheSenderWorksBothWays();
	checkHandleOfTheReceiverWorksBothWays();
	checkAnUnboundEndpointOpensNothing();
	checkAHandleDoesNotOpenAThirdProcess();
	checkRevocationCutsBothDirections();
	checkLabelIsSetOnce();
	checkNodesLeaveWithTheirHolders();
	checkSpawnGivesAHandleBadgedWithTheNewProcess();
	checkSpawnWithoutAHandleLeavesNone();
	checkSpawnRefusesAHandleTheCreatorMayNotMint();
	TestRunner::end_suite();
}

} // namespace cock::test::security
