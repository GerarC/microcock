#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_transfer.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/revocation.hpp>
#include <cock/abi/call.hpp>
#include <cock/core/syscall/ipc/endpoint_route.hpp>
#include <cock/core/task/endpoint_table.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/reply_table.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <cock/ipc/message.hpp>
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
using core::hal::SyscallFrame;
using core::syscall::EndpointRoute;
using core::syscall::resolveHandle;
using core::task::AcceptResult;
using core::task::CallStart;
using core::task::endpoint_table;
using core::task::EndpointTable;
using core::task::IPC;
using core::task::Process;
using core::task::reply_table;
using core::task::Scheduler;
using core::task::SyncIpc;
using core::task::TaskManager;
using core::task::Timeouts;
using ipc::Message;
using ipc::MessageType;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
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
constexpr uint32_t FIRST_VALUE = 0x61;
constexpr uint32_t SECOND_VALUE = 0x62;
constexpr size_t QUEUED_AFTER_TWO_POSTS = 2;
constexpr size_t NO_MESSAGES = 0;
constexpr uintptr_t FAKE_STACK_POINTER = 0x1000;
constexpr size_t DRAIN_LIMIT = 64;

Thread *idleThread = nullptr;
constexpr size_t ONE_MESSAGE = 1;
constexpr uint32_t NO_REQUESTER = 0;
constexpr size_t PEER_VALUE = 0;
constexpr size_t FIRST_WORD_VALUE = 1;
constexpr uint32_t SHORT_WAIT_MILLISECONDS = 50;
constexpr uint32_t NO_WAIT_MILLISECONDS = 0;
constexpr uint64_t SETTLE_TICKS = 100;
constexpr uint32_t FIRST_REQUEST[abi::call::WORD_COUNT] = {0x11, 0x12, 0x13, 0x14};
constexpr uint32_t SECOND_REQUEST[abi::call::WORD_COUNT] = {0x21, 0x22, 0x23, 0x24};
constexpr uint32_t FIRST_REPLY[abi::call::WORD_COUNT] = {0xA1, 0xA2, 0xA3, 0xA4};
constexpr uint32_t SECOND_REPLY[abi::call::WORD_COUNT] = {0xB1, 0xB2, 0xB3, 0xB4};

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

ThreadResult idleEntry() { return ThreadResult::success(); }

void retire(Thread *thread) {
	thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(thread);
}

void startScheduler() {
	Scheduler::init();
	Scheduler::setIdleThread(idleThread);
}

void drain() {
	for (size_t round = 0; round < DRAIN_LIMIT; round++) {
		Thread *current = Scheduler::getCurrentThread();
		if (current == idleThread) return;
		if (current != nullptr) current->setState(ThreadState::BLOCKED);
		Scheduler::onYield(FAKE_STACK_POINTER);
	}
}

Message makeMessage(MessageType type, uint32_t value) {
	Message message = {};
	message.type = type;
	message.badge = CLIENT_BADGE;
	message.payload.args.arg0 = value;
	return message;
}

void checkResolveNamesAnUnboundEndpoint() {
	Party minter = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(minter, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	CapabilityNodeId node = delegate_capability(*minter.process, *client.process, CapabilityType::ENDPOINT,
												endpoint, SINGLE_ENDPOINT, false, false, CLIENT_BADGE);

	EndpointRoute route = resolveHandle(*client.thread, node);
	EXPECT_EQ(route.endpoint, endpoint);
	EXPECT_EQ(route.badge, CLIENT_BADGE);
	EXPECT_TRUE(route.target == nullptr);

	EXPECT_EQ(resolveHandle(*client.thread, UNKNOWN_HANDLE).endpoint, NO_ENDPOINT);
	EXPECT_TRUE(revoke_capability(*minter.process, node));
	EXPECT_EQ(resolveHandle(*client.thread, node).endpoint, NO_ENDPOINT);

	retire(client);
	retire(minter);
}

void checkMessagesWaitForTheNextOwner() {
	Party first = makeParty();
	Party second = makeParty();
	Party sender = makeParty();
	uint32_t endpoint = createFor(sender, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	uint32_t token = reply_table.openQueued(sender.thread->getId(), sender.id());
	Message request = makeMessage(MessageType::SYS_PING, FIRST_VALUE);
	request.reply = token;

	EXPECT_TRUE(endpoint_table.bind(endpoint, first.id()));
	EXPECT_TRUE(IPC::post(endpoint, request));
	EXPECT_TRUE(first.thread->hasMessages());
	EXPECT_FALSE(second.thread->hasMessages());
	retire(first);

	EXPECT_TRUE(IPC::post(endpoint, makeMessage(MessageType::SYS_PONG, SECOND_VALUE)));
	EXPECT_EQ(endpoint_table.queuedCount(endpoint), QUEUED_AFTER_TWO_POSTS);
	EXPECT_FALSE(reply_table.holds(token, second.id()));

	EXPECT_TRUE(endpoint_table.bind(endpoint, second.id()));
	EXPECT_TRUE(second.thread->hasMessages());
	Message taken = {};
	EXPECT_TRUE(second.thread->getNextMessage(taken));
	EXPECT_EQ(taken.payload.args.arg0, FIRST_VALUE);
	EXPECT_EQ(taken.badge, CLIENT_BADGE);
	EXPECT_EQ(taken.target_pid, second.thread->getId());
	EXPECT_EQ(taken.reply, token);
	EXPECT_TRUE(reply_table.holds(token, second.id()));
	EXPECT_TRUE(second.thread->getNextMessage(taken));
	EXPECT_EQ(taken.payload.args.arg0, SECOND_VALUE);
	EXPECT_FALSE(second.thread->getNextMessage(taken));
	EXPECT_FALSE(second.thread->hasMessages());

	reply_table.close(token);
	retire(second);
	retire(sender);
}

void checkAFullQueueRefusesAndOnlyTheOwnerDrainsIt() {
	Party owner = makeParty();
	Party other = makeParty();
	uint32_t endpoint = createFor(owner, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	EXPECT_TRUE(endpoint_table.bind(endpoint, owner.id()));

	for (size_t index = 0; index < abi::endpoint::MAX_QUEUED_PER_ENDPOINT; index++)
		EXPECT_TRUE(IPC::post(endpoint, makeMessage(MessageType::SYS_PING, FIRST_VALUE)));
	EXPECT_FALSE(IPC::post(endpoint, makeMessage(MessageType::SYS_PING, FIRST_VALUE)));

	Message taken = {};
	EXPECT_FALSE(other.thread->hasMessages());
	EXPECT_FALSE(other.thread->getNextMessage(taken));
	for (size_t index = 0; index < abi::endpoint::MAX_QUEUED_PER_ENDPOINT; index++)
		EXPECT_TRUE(owner.thread->getNextMessage(taken));
	EXPECT_FALSE(owner.thread->getNextMessage(taken));
	EXPECT_EQ(endpoint_table.queuedMessages(), NO_MESSAGES);
	EXPECT_TRUE(IPC::post(endpoint, makeMessage(MessageType::SYS_PING, FIRST_VALUE)));
	EXPECT_TRUE(owner.thread->getNextMessage(taken));

	retire(other);
	retire(owner);
}

void checkCapabilityMessagesDoNotPassToANewOwner() {
	Party first = makeParty();
	Party second = makeParty();
	Party sender = makeParty();
	uint32_t endpoint = createFor(sender, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);
	uint32_t token = reply_table.openQueued(sender.thread->getId(), sender.id());
	Message transfer = makeMessage(MessageType::CAPABILITY_TRANSFER, FIRST_VALUE);
	transfer.reply = token;

	EXPECT_TRUE(endpoint_table.bind(endpoint, first.id()));
	EXPECT_TRUE(IPC::post(endpoint, transfer));
	EXPECT_TRUE(IPC::post(endpoint, makeMessage(MessageType::SYS_PING, SECOND_VALUE)));
	EXPECT_TRUE(endpoint_table.bind(endpoint, second.id(), core::task::closeTokenOfMessage));

	EXPECT_EQ(endpoint_table.queuedCount(endpoint), ONE_MESSAGE);
	EXPECT_EQ(reply_table.requesterOf(token), NO_REQUESTER);
	Message taken = {};
	EXPECT_TRUE(second.thread->getNextMessage(taken));
	EXPECT_EQ(taken.payload.args.arg0, SECOND_VALUE);
	EXPECT_FALSE(second.thread->getNextMessage(taken));

	retire(second);
	retire(first);
	retire(sender);
}

void checkCallsWaitForTheNextOwner() {
	startScheduler();
	Party first = makeParty();
	Party second = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(client, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);

	EXPECT_TRUE(SyncIpc::callThroughEndpoint(*client.thread, endpoint, FIRST_REQUEST, abi::call::NO_TIMEOUT,
											 CLIENT_BADGE) == CallStart::BLOCKED);
	EXPECT_TRUE(client.thread->getState() == ThreadState::WAITING_REPLY);
	EXPECT_EQ(client.thread->callEndpoint, endpoint);
	EXPECT_TRUE(client.thread->callTarget == nullptr);
	EXPECT_FALSE(SyncIpc::hasCallers(*first.thread));

	EXPECT_TRUE(endpoint_table.bind(endpoint, first.id()));
	EXPECT_TRUE(SyncIpc::hasCallers(*first.thread));
	EXPECT_FALSE(SyncIpc::hasCallers(*second.thread));
	retire(first);
	EXPECT_TRUE(client.thread->getState() == ThreadState::WAITING_REPLY);

	EXPECT_TRUE(endpoint_table.bind(endpoint, second.id()));
	EXPECT_TRUE(SyncIpc::hasCallers(*second.thread));
	EXPECT_TRUE(SyncIpc::accept(*second.thread, abi::call::NO_TIMEOUT) == AcceptResult::SERVED);
	EXPECT_FALSE(SyncIpc::hasCallers(*second.thread));
	EXPECT_TRUE(second.thread->serving == client.thread);
	EXPECT_TRUE(client.thread->callTarget == second.thread);
	EXPECT_EQ(client.thread->callEndpoint, NO_ENDPOINT);

	SyscallFrame request = {};
	core::hal::read_syscall_frame(second.thread->getStackPointer(), request);
	EXPECT_EQ(request.result, abi::call::SUCCESS);
	EXPECT_EQ(request.values[PEER_VALUE], static_cast<uintptr_t>(CLIENT_BADGE));
	EXPECT_EQ(request.values[FIRST_WORD_VALUE], static_cast<uintptr_t>(FIRST_REQUEST[0]));

	EXPECT_TRUE(SyncIpc::reply(*second.thread, FIRST_REPLY, false));
	EXPECT_TRUE(client.thread->getState() == ThreadState::READY);

	Scheduler::takeRescheduleRequest();
	drain();
	retire(second);
	retire(client);
	TaskManager::buryDeadThreads();
}

void checkAQueuedCallTimesOutAndLeavesTheQueue() {
	startScheduler();
	Party server = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(client, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);

	EXPECT_TRUE(SyncIpc::callThroughEndpoint(*client.thread, endpoint, FIRST_REQUEST, SHORT_WAIT_MILLISECONDS,
											 CLIENT_BADGE) == CallStart::BLOCKED);
	for (uint64_t tick = 0; tick < SETTLE_TICKS; tick++)
		Timeouts::onTick();

	EXPECT_TRUE(client.thread->getState() == ThreadState::READY);
	EXPECT_EQ(client.thread->callEndpoint, NO_ENDPOINT);
	SyscallFrame answer = {};
	core::hal::read_syscall_frame(client.thread->getStackPointer(), answer);
	EXPECT_EQ(answer.result, abi::call::TIMED_OUT);

	EXPECT_TRUE(endpoint_table.bind(endpoint, server.id()));
	EXPECT_FALSE(SyncIpc::hasCallers(*server.thread));
	EXPECT_TRUE(SyncIpc::callThroughEndpoint(*client.thread, endpoint, FIRST_REQUEST, NO_WAIT_MILLISECONDS,
											 CLIENT_BADGE) == CallStart::TIMED_OUT);
	EXPECT_TRUE(SyncIpc::callThroughEndpoint(*client.thread, NO_ENDPOINT, FIRST_REQUEST,
											 abi::call::NO_TIMEOUT, CLIENT_BADGE) == CallStart::REFUSED);

	Scheduler::takeRescheduleRequest();
	drain();
	retire(server);
	retire(client);
	TaskManager::buryDeadThreads();
}

void checkARestartedServerTakesTheCallerThatWaitedAsleep() {
	startScheduler();
	Party first = makeParty();
	Party second = makeParty();
	Party client = makeParty();
	uint32_t endpoint = createFor(client, CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT);

	EXPECT_TRUE(endpoint_table.bind(endpoint, first.id()));
	EXPECT_TRUE(SyncIpc::accept(*first.thread, abi::call::NO_TIMEOUT) == AcceptResult::BLOCKED);
	EXPECT_TRUE(first.thread->getState() == ThreadState::WAITING_CALL);
	retire(first);

	EXPECT_TRUE(SyncIpc::callThroughEndpoint(*client.thread, endpoint, SECOND_REQUEST, abi::call::NO_TIMEOUT,
											 CLIENT_BADGE) == CallStart::BLOCKED);
	EXPECT_TRUE(client.thread->getState() == ThreadState::WAITING_REPLY);

	EXPECT_TRUE(endpoint_table.bind(endpoint, second.id()));
	EXPECT_TRUE(SyncIpc::accept(*second.thread, abi::call::NO_TIMEOUT) == AcceptResult::SERVED);
	EXPECT_TRUE(SyncIpc::reply(*second.thread, SECOND_REPLY, false));
	EXPECT_TRUE(client.thread->getState() == ThreadState::READY);

	Scheduler::takeRescheduleRequest();
	drain();
	retire(second);
	retire(client);
	TaskManager::buryDeadThreads();
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
	idleThread = new Thread(idleEntry, ThreadPriority::IDLE);
	checkResolveNamesAnUnboundEndpoint();
	checkMessagesWaitForTheNextOwner();
	checkAFullQueueRefusesAndOnlyTheOwnerDrainsIt();
	checkCapabilityMessagesDoNotPassToANewOwner();
	checkCallsWaitForTheNextOwner();
	checkAQueuedCallTimesOutAndLeavesTheQueue();
	checkARestartedServerTakesTheCallerThatWaitedAsleep();
	retire(idleThread);
	TaskManager::buryDeadThreads();
	idleThread = nullptr;
	endpoint_table.clear();
	TestRunner::end_suite();
}

} // namespace cock::test::security
