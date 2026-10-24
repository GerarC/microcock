#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_transfer.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/revocation.hpp>
#include <cock/core/syscall/ipc/endpoint_route.hpp>
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
using core::security::CAPABILITY_FLAG_BIND;
using core::security::CAPABILITY_FLAG_GRANT;
using core::security::CAPABILITY_FLAG_MINT;
using core::security::delegate_capability;
using core::security::NO_CAPABILITY_NODE;
using core::security::revoke_capability;
using core::syscall::EndpointRoute;
using core::syscall::resolveHandle;
using core::task::endpoint_table;
using core::task::EndpointTable;
using core::task::Process;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadState;
using abi::endpoint::FIRST_ENDPOINT;
using abi::endpoint::NO_BADGE;
using abi::endpoint::NO_ENDPOINT;
using abi::endpoint::SINGLE_ENDPOINT;

namespace {

constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr uint32_t CLIENT_BADGE = 0x51;
constexpr uint32_t OTHER_BADGE = 0x52;
constexpr uint32_t UNKNOWN_HANDLE = 0x7FFFFFF0;
constexpr uint64_t IO_PORT = 0x3F8;
constexpr uint64_t IO_PORT_COUNT = 8;
constexpr uint32_t SECOND_ENDPOINT_OFFSET = 1;
constexpr uint32_t PAST_LAST_OFFSET = 2;
constexpr uint32_t NEXT_NODE_OFFSET = 1;
constexpr uint32_t UNBOUND_OWNER = 0;
constexpr uint32_t NO_FLAGS = 0;
constexpr uint32_t FIRST_OWNER = 5;
constexpr uint32_t SECOND_OWNER = 6;
constexpr uint32_t THIRD_OWNER = 9;

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

uint32_t createFor(Party &owner, uint32_t flags) {
	uint32_t id = endpoint_table.create();
	Capability capability;
	capability.type = CapabilityType::ENDPOINT;
	capability.flags = flags;
	capability.first = id;
	capability.count = SINGLE_ENDPOINT;
	capability.node = capability_tree.createRoot(owner.id(), CapabilityType::ENDPOINT, id, SINGLE_ENDPOINT);
	owner.process->getCapabilities().add(capability);
	return id;
}

void checkTableCreatesInOrderAndBinds() {
	EndpointTable table;
	uint32_t first = table.create();
	uint32_t second = table.create();
	uint32_t past_last = first + PAST_LAST_OFFSET;
	EXPECT_EQ(first, FIRST_ENDPOINT);
	EXPECT_EQ(second, FIRST_ENDPOINT + SECOND_ENDPOINT_OFFSET);
	EXPECT_TRUE(table.exists(first));
	EXPECT_FALSE(table.exists(NO_ENDPOINT));
	EXPECT_FALSE(table.exists(past_last));

	EXPECT_EQ(table.ownerOf(first), UNBOUND_OWNER);
	EXPECT_TRUE(table.bind(first, FIRST_OWNER));
	EXPECT_EQ(table.ownerOf(first), FIRST_OWNER);
	EXPECT_TRUE(table.bind(first, SECOND_OWNER));
	EXPECT_EQ(table.ownerOf(first), SECOND_OWNER);
	EXPECT_FALSE(table.bind(first, UNBOUND_OWNER));
	EXPECT_FALSE(table.bind(past_last, FIRST_OWNER));
	EXPECT_FALSE(table.bind(NO_ENDPOINT, FIRST_OWNER));
}

void checkTableFillsAndReleases() {
	EndpointTable table;
	for (size_t index = 0; index < EndpointTable::MAX_ENDPOINTS; index++)
		EXPECT_NEQ(table.create(), NO_ENDPOINT);
	EXPECT_EQ(table.create(), NO_ENDPOINT);

	EXPECT_TRUE(table.bind(FIRST_ENDPOINT, THIRD_OWNER));
	table.releaseOwner(THIRD_OWNER);
	EXPECT_EQ(table.ownerOf(FIRST_ENDPOINT), UNBOUND_OWNER);
	EXPECT_TRUE(table.exists(FIRST_ENDPOINT));

	EXPECT_TRUE(table.destroy(FIRST_ENDPOINT));
	EXPECT_FALSE(table.exists(FIRST_ENDPOINT));
	EXPECT_FALSE(table.destroy(FIRST_ENDPOINT));
	EXPECT_EQ(table.create(), FIRST_ENDPOINT);
}

void checkMinterChoosesTheBadge() {
	Party owner = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(owner, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);

	CapabilityNodeId node = delegate_capability(*owner.process, *client.process, CapabilityType::ENDPOINT,
												endpoint, SINGLE_ENDPOINT, false, false, CLIENT_BADGE);
	EXPECT_NEQ(node, NO_CAPABILITY_NODE);
	const Capability *held = client.process->getCapabilities().endpointHandle(node);
	EXPECT_NOT_NULL(held);
	if (held != nullptr) {
		EXPECT_EQ(held->badge, CLIENT_BADGE);
		EXPECT_EQ(held->first, static_cast<uint64_t>(endpoint));
		EXPECT_EQ(held->flags & CAPABILITY_FLAG_MINT, NO_FLAGS);
	}

	retire(client);
	retire(owner);
}

void checkHolderWithoutMintCannotChooseBadges() {
	Party owner = makeParty();
	Party client = makeParty();
	Party third = makeParty();
	uint32_t endpoint = createFor(owner, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	EXPECT_NEQ(delegate_capability(*owner.process, *client.process, CapabilityType::ENDPOINT, endpoint,
								   SINGLE_ENDPOINT, true, false, CLIENT_BADGE),
			   NO_CAPABILITY_NODE);

	EXPECT_EQ(delegate_capability(*client.process, *third.process, CapabilityType::ENDPOINT, endpoint,
								  SINGLE_ENDPOINT, false, false, OTHER_BADGE),
			  NO_CAPABILITY_NODE);
	EXPECT_EQ(delegate_capability(*client.process, *third.process, CapabilityType::ENDPOINT, endpoint,
								  SINGLE_ENDPOINT, false, true, NO_BADGE),
			  NO_CAPABILITY_NODE);

	CapabilityNodeId passed = delegate_capability(*client.process, *third.process, CapabilityType::ENDPOINT,
												  endpoint, SINGLE_ENDPOINT, false, false, NO_BADGE);
	EXPECT_NEQ(passed, NO_CAPABILITY_NODE);
	const Capability *held = third.process->getCapabilities().endpointHandle(passed);
	EXPECT_NOT_NULL(held);
	if (held != nullptr) EXPECT_EQ(held->badge, CLIENT_BADGE);

	retire(third);
	retire(client);
	retire(owner);
}

void checkMintRightPassesOnlyFromAMinter() {
	Party owner = makeParty();
	Party directory = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(owner, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);

	EXPECT_NEQ(delegate_capability(*owner.process, *directory.process, CapabilityType::ENDPOINT, endpoint,
								   SINGLE_ENDPOINT, true, true, NO_BADGE),
			   NO_CAPABILITY_NODE);
	CapabilityNodeId minted = delegate_capability(*directory.process, *client.process, CapabilityType::ENDPOINT,
												  endpoint, SINGLE_ENDPOINT, false, false, OTHER_BADGE);
	EXPECT_NEQ(minted, NO_CAPABILITY_NODE);
	const Capability *held = client.process->getCapabilities().endpointHandle(minted);
	EXPECT_NOT_NULL(held);
	if (held != nullptr) EXPECT_EQ(held->badge, OTHER_BADGE);

	retire(client);
	retire(directory);
	retire(owner);
}

void checkBindRightIsNeverPassedOn() {
	Party owner = makeParty();
	Party directory = makeParty();
	uint32_t endpoint = createFor(owner, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT | CAPABILITY_FLAG_BIND);
	EXPECT_TRUE(owner.process->getCapabilities().hasFlag(CapabilityType::ENDPOINT, endpoint, SINGLE_ENDPOINT,
														 CAPABILITY_FLAG_BIND));

	EXPECT_NEQ(delegate_capability(*owner.process, *directory.process, CapabilityType::ENDPOINT, endpoint,
								   SINGLE_ENDPOINT, true, true, NO_BADGE),
			   NO_CAPABILITY_NODE);
	EXPECT_FALSE(directory.process->getCapabilities().hasFlag(CapabilityType::ENDPOINT, endpoint,
															  SINGLE_ENDPOINT, CAPABILITY_FLAG_BIND));

	retire(directory);
	retire(owner);
}

void checkBadgeAndMintAreRefusedForOtherTypes() {
	Party giver = makeParty();
	Party receiver = makeParty();
	Capability ports;
	ports.type = CapabilityType::IO_PORTS;
	ports.flags = CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT;
	ports.first = IO_PORT;
	ports.count = IO_PORT_COUNT;
	ports.node = capability_tree.createRoot(giver.id(), CapabilityType::IO_PORTS, IO_PORT, IO_PORT_COUNT);
	giver.process->getCapabilities().add(ports);

	EXPECT_EQ(delegate_capability(*giver.process, *receiver.process, CapabilityType::IO_PORTS, IO_PORT,
								  IO_PORT_COUNT, false, false, CLIENT_BADGE),
			  NO_CAPABILITY_NODE);
	EXPECT_EQ(delegate_capability(*giver.process, *receiver.process, CapabilityType::IO_PORTS, IO_PORT,
								  IO_PORT_COUNT, false, true, NO_BADGE),
			  NO_CAPABILITY_NODE);
	EXPECT_NEQ(delegate_capability(*giver.process, *receiver.process, CapabilityType::IO_PORTS, IO_PORT,
								   IO_PORT_COUNT, false, false, NO_BADGE),
			   NO_CAPABILITY_NODE);

	retire(receiver);
	retire(giver);
}

void checkHandlesGiveNoGrantAuthority() {
	Party directory = makeParty();
	createFor(directory, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	EXPECT_FALSE(directory.process->getCapabilities().holdsGrantAuthority());
	retire(directory);
}

void checkHandleLookupIsByNodeAndType() {
	Party owner = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(owner, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	CapabilityNodeId node = delegate_capability(*owner.process, *client.process, CapabilityType::ENDPOINT,
												endpoint, SINGLE_ENDPOINT, false, false, CLIENT_BADGE);

	EXPECT_NOT_NULL(client.process->getCapabilities().endpointHandle(node));
	EXPECT_TRUE(client.process->getCapabilities().endpointHandle(NO_CAPABILITY_NODE) == nullptr);
	EXPECT_TRUE(client.process->getCapabilities().endpointHandle(UNKNOWN_HANDLE) == nullptr);
	EXPECT_TRUE(client.process->getCapabilities().endpointHandle(node + NEXT_NODE_OFFSET) == nullptr);

	retire(client);
	retire(owner);
}

void checkResolveFindsTheOwnerAndTheBadge() {
	Party server = makeParty();
	Party minter = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(minter, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	CapabilityNodeId node = delegate_capability(*minter.process, *client.process, CapabilityType::ENDPOINT,
												endpoint, SINGLE_ENDPOINT, false, false, CLIENT_BADGE);

	EXPECT_TRUE(resolveHandle(*client.thread, node).target == nullptr);

	EXPECT_TRUE(endpoint_table.bind(endpoint, server.id()));
	EndpointRoute route = resolveHandle(*client.thread, node);
	EXPECT_TRUE(route.target == server.thread);
	EXPECT_EQ(route.badge, CLIENT_BADGE);

	EXPECT_TRUE(resolveHandle(*server.thread, node).target == nullptr);
	EXPECT_TRUE(resolveHandle(*client.thread, UNKNOWN_HANDLE).target == nullptr);
	EXPECT_TRUE(resolveHandle(*client.thread, NO_CAPABILITY_NODE).target == nullptr);

	retire(client);
	retire(minter);
	retire(server);
}

void checkRestartedServerKeepsTheHandles() {
	Party first = makeParty();
	Party second = makeParty();
	Party minter = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(minter, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	CapabilityNodeId node = delegate_capability(*minter.process, *client.process, CapabilityType::ENDPOINT,
												endpoint, SINGLE_ENDPOINT, false, false, CLIENT_BADGE);
	EXPECT_TRUE(endpoint_table.bind(endpoint, first.id()));
	EXPECT_TRUE(resolveHandle(*client.thread, node).target == first.thread);

	retire(first);
	EXPECT_TRUE(resolveHandle(*client.thread, node).target == nullptr);

	EXPECT_TRUE(endpoint_table.bind(endpoint, second.id()));
	EndpointRoute route = resolveHandle(*client.thread, node);
	EXPECT_TRUE(route.target == second.thread);
	EXPECT_EQ(route.badge, CLIENT_BADGE);

	retire(client);
	retire(minter);
	retire(second);
}

void checkRevokedHandleLeadsNowhere() {
	Party server = makeParty();
	Party minter = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(minter, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	CapabilityNodeId node = delegate_capability(*minter.process, *client.process, CapabilityType::ENDPOINT,
												endpoint, SINGLE_ENDPOINT, false, false, CLIENT_BADGE);
	EXPECT_TRUE(endpoint_table.bind(endpoint, server.id()));
	EXPECT_TRUE(resolveHandle(*client.thread, node).target == server.thread);

	EXPECT_TRUE(revoke_capability(*minter.process, node));
	EXPECT_TRUE(resolveHandle(*client.thread, node).target == nullptr);
	EXPECT_TRUE(client.process->getCapabilities().endpointHandle(node) == nullptr);

	retire(client);
	retire(minter);
	retire(server);
}

} // namespace

void test_endpoint() {
	TestRunner::begin_suite("Endpoints");
	checkTableCreatesInOrderAndBinds();
	checkTableFillsAndReleases();
	checkMinterChoosesTheBadge();
	checkHolderWithoutMintCannotChooseBadges();
	checkMintRightPassesOnlyFromAMinter();
	checkBindRightIsNeverPassedOn();
	checkBadgeAndMintAreRefusedForOtherTypes();
	checkHandlesGiveNoGrantAuthority();
	checkHandleLookupIsByNodeAndType();
	checkResolveFindsTheOwnerAndTheBadge();
	checkRestartedServerKeepsTheHandles();
	checkRevokedHandleLeadsNowhere();
	endpoint_table.clear();
	TestRunner::end_suite();
}

} // namespace cock::test::security
