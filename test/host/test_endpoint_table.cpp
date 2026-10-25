#include <cock/core/security/capability.hpp>
#include <cock/core/task/endpoint_table.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using cock::abi::endpoint::FIRST_ENDPOINT;
using cock::abi::endpoint::MAX_QUEUED_PER_ENDPOINT;
using cock::abi::endpoint::NO_ENDPOINT;
using cock::abi::endpoint::NO_REPLY;
using cock::abi::endpoint::QUEUED_MESSAGE_POOL;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::abi::endpoint::SINGLE_ENDPOINT;
using cock::core::security::Capability;
using cock::core::security::CAPABILITY_FLAG_BIND;
using cock::core::security::CAPABILITY_FLAG_GRANT;
using cock::core::security::CAPABILITY_FLAG_MINT;
using cock::core::security::CapabilitySet;
using cock::core::security::CapabilityType;
using cock::core::security::NO_CAPABILITY_NODE;
using cock::core::task::EndpointTable;

namespace {

constexpr uint32_t UNBOUND = 0;
constexpr uint32_t FIRST_OWNER = 4;
constexpr uint32_t SECOND_OWNER = 8;
constexpr uint32_t OTHER_OWNER = 12;
constexpr uint32_t NEXT_ID_OFFSET = 1;
constexpr uint32_t FIRST_NODE = 21;
constexpr uint32_t SECOND_NODE = 22;
constexpr uint32_t THIRD_NODE = 23;
constexpr uint32_t UNKNOWN_NODE = 99;
constexpr uint32_t NO_FLAGS = 0;
constexpr uint32_t BADGE = 0x77;
constexpr uint64_t IO_FIRST = 0x60;
constexpr uint64_t IO_COUNT = 4;
constexpr uint32_t FIRST_VALUE = 0x61;
constexpr uint32_t SECOND_VALUE = 0x62;
constexpr uint32_t THIRD_VALUE = 0x63;
constexpr uint32_t FOURTH_VALUE = 0x64;
constexpr uint32_t FIRST_TOKEN = 0x101;
constexpr uint32_t SECOND_TOKEN = 0x102;
constexpr size_t NONE = 0;
constexpr size_t DROPPED_TRANSFERS = 2;
constexpr size_t KEPT_MESSAGES = 2;
constexpr size_t LAST_INDEX_OFFSET = 1;
constexpr size_t POOL_ENDPOINTS = QUEUED_MESSAGE_POOL / MAX_QUEUED_PER_ENDPOINT + 1;
constexpr size_t QUEUE_SCRATCH = 4;

Capability endpointHandle(uint32_t flags, uint32_t endpoint, uint32_t node, uint32_t badge) {
	Capability capability;
	capability.type = CapabilityType::ENDPOINT;
	capability.flags = flags;
	capability.first = endpoint;
	capability.count = SINGLE_ENDPOINT;
	capability.node = node;
	capability.badge = badge;
	return capability;
}

void checkIdsFollowInOrder() {
	EndpointTable table;
	uint32_t first = table.create();
	uint32_t second = table.create();
	CHECK_EQ(FIRST_ENDPOINT, first);
	CHECK_EQ(first + NEXT_ID_OFFSET, second);
	CHECK_TRUE(table.exists(first));
	CHECK_TRUE(table.exists(second));
	CHECK_FALSE(table.exists(NO_ENDPOINT));
	CHECK_FALSE(table.exists(second + NEXT_ID_OFFSET));
}

void checkBindAndRelease() {
	EndpointTable table;
	uint32_t id = table.create();
	CHECK_EQ(UNBOUND, table.ownerOf(id));
	CHECK_TRUE(table.bind(id, FIRST_OWNER));
	CHECK_EQ(FIRST_OWNER, table.ownerOf(id));
	CHECK_TRUE(table.bind(id, SECOND_OWNER));
	CHECK_EQ(SECOND_OWNER, table.ownerOf(id));

	CHECK_FALSE(table.bind(id, UNBOUND));
	CHECK_FALSE(table.bind(NO_ENDPOINT, FIRST_OWNER));
	CHECK_FALSE(table.bind(id + NEXT_ID_OFFSET, FIRST_OWNER));

	table.releaseOwner(OTHER_OWNER);
	CHECK_EQ(SECOND_OWNER, table.ownerOf(id));
	table.releaseOwner(UNBOUND);
	CHECK_EQ(SECOND_OWNER, table.ownerOf(id));
	table.releaseOwner(SECOND_OWNER);
	CHECK_EQ(UNBOUND, table.ownerOf(id));
	CHECK_TRUE(table.exists(id));
}

void checkFullTableAndDestroy() {
	EndpointTable table;
	for (size_t index = 0; index < EndpointTable::MAX_ENDPOINTS; index++)
		CHECK_TRUE(table.create() != NO_ENDPOINT);
	CHECK_EQ(NO_ENDPOINT, table.create());

	CHECK_TRUE(table.destroy(FIRST_ENDPOINT));
	CHECK_FALSE(table.exists(FIRST_ENDPOINT));
	CHECK_FALSE(table.destroy(FIRST_ENDPOINT));
	CHECK_FALSE(table.destroy(NO_ENDPOINT));
	CHECK_EQ(FIRST_ENDPOINT, table.create());

	table.clear();
	CHECK_FALSE(table.exists(FIRST_ENDPOINT));
}

void checkHandleLookupIsByNodeAndType() {
	CapabilitySet set;
	CHECK_TRUE(set.add(endpointHandle(NO_FLAGS, FIRST_ENDPOINT, FIRST_NODE, BADGE)));
	Capability ports;
	ports.type = CapabilityType::IO_PORTS;
	ports.first = IO_FIRST;
	ports.count = IO_COUNT;
	ports.node = SECOND_NODE;
	CHECK_TRUE(set.add(ports));

	const Capability *found = set.endpointHandle(FIRST_NODE);
	CHECK_TRUE(found != nullptr);
	if (found != nullptr) {
		CHECK_EQ(BADGE, found->badge);
		CHECK_EQ(static_cast<uint64_t>(FIRST_ENDPOINT), found->first);
	}
	CHECK_TRUE(set.endpointHandle(SECOND_NODE) == nullptr);
	CHECK_TRUE(set.endpointHandle(UNKNOWN_NODE) == nullptr);
	CHECK_TRUE(set.endpointHandle(NO_CAPABILITY_NODE) == nullptr);
	CHECK_TRUE(set.removeNode(FIRST_NODE));
	CHECK_TRUE(set.endpointHandle(FIRST_NODE) == nullptr);
}

void checkFlagQueries() {
	CapabilitySet set;
	CHECK_TRUE(set.add(endpointHandle(CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT, FIRST_ENDPOINT,
									  FIRST_NODE, BADGE)));
	CHECK_TRUE(set.add(endpointHandle(CAPABILITY_FLAG_BIND, FIRST_ENDPOINT + NEXT_ID_OFFSET, SECOND_NODE,
									  BADGE)));

	CHECK_TRUE(set.hasFlag(CapabilityType::ENDPOINT, FIRST_ENDPOINT, SINGLE_ENDPOINT, CAPABILITY_FLAG_MINT));
	CHECK_FALSE(set.hasFlag(CapabilityType::ENDPOINT, FIRST_ENDPOINT, SINGLE_ENDPOINT, CAPABILITY_FLAG_BIND));
	CHECK_TRUE(set.hasFlag(CapabilityType::ENDPOINT, FIRST_ENDPOINT + NEXT_ID_OFFSET, SINGLE_ENDPOINT,
						   CAPABILITY_FLAG_BIND));
	CHECK_FALSE(set.hasFlag(CapabilityType::ENDPOINT, FIRST_ENDPOINT + NEXT_ID_OFFSET, SINGLE_ENDPOINT,
							CAPABILITY_FLAG_MINT));
	CHECK_FALSE(set.hasFlag(CapabilityType::ENDPOINT, NO_ENDPOINT, SINGLE_ENDPOINT, CAPABILITY_FLAG_MINT));
	CHECK_TRUE(set.mayGrant(CapabilityType::ENDPOINT, FIRST_ENDPOINT, SINGLE_ENDPOINT));
	CHECK_FALSE(set.mayGrant(CapabilityType::ENDPOINT, FIRST_ENDPOINT + NEXT_ID_OFFSET, SINGLE_ENDPOINT));
}

void checkHandlesGiveNoGrantAuthority() {
	CapabilitySet set;
	CHECK_TRUE(set.add(endpointHandle(CAPABILITY_FLAG_GRANT | CAPABILITY_FLAG_MINT, FIRST_ENDPOINT,
									  THIRD_NODE, BADGE)));
	CHECK_FALSE(set.holdsGrantAuthority());

	Capability ports;
	ports.type = CapabilityType::IO_PORTS;
	ports.flags = CAPABILITY_FLAG_GRANT;
	ports.first = IO_FIRST;
	ports.count = IO_COUNT;
	CHECK_TRUE(set.add(ports));
	CHECK_TRUE(set.holdsGrantAuthority());
}

uint32_t droppedReplies[QUEUE_SCRATCH];
size_t droppedCount = 0;

void recordDrop(const Message &message) {
	if (droppedCount < QUEUE_SCRATCH) droppedReplies[droppedCount] = message.reply;
	droppedCount++;
}

Message messageWith(MessageType type, uint32_t value, uint32_t reply) {
	Message message = {};
	message.type = type;
	message.reply = reply;
	message.payload.args.arg0 = value;
	return message;
}

void checkQueueKeepsOrderAndLimits() {
	EndpointTable table;
	uint32_t id = table.create();
	Message out = {};
	CHECK_FALSE(table.dequeue(id, out));
	CHECK_FALSE(table.enqueue(NO_ENDPOINT, messageWith(MessageType::SYS_PING, FIRST_VALUE, NO_REPLY)));

	for (size_t index = 0; index < MAX_QUEUED_PER_ENDPOINT; index++)
		CHECK_TRUE(table.enqueue(id, messageWith(MessageType::SYS_PING, FIRST_VALUE + static_cast<uint32_t>(index), NO_REPLY)));
	CHECK_FALSE(table.enqueue(id, messageWith(MessageType::SYS_PING, FIRST_VALUE, NO_REPLY)));
	CHECK_EQ(MAX_QUEUED_PER_ENDPOINT, table.queuedCount(id));

	for (size_t index = 0; index < MAX_QUEUED_PER_ENDPOINT; index++) {
		CHECK_TRUE(table.dequeue(id, out));
		CHECK_EQ(FIRST_VALUE + static_cast<uint32_t>(index), out.payload.args.arg0);
	}
	CHECK_FALSE(table.dequeue(id, out));
	CHECK_EQ(NONE, table.queuedMessages());
}

void checkPoolIsSharedAndCellsAreReused() {
	EndpointTable table;
	size_t queued = 0;
	uint32_t ids[POOL_ENDPOINTS];
	for (size_t index = 0; index < POOL_ENDPOINTS; index++)
		ids[index] = table.create();

	for (size_t index = 0; index < POOL_ENDPOINTS; index++) {
		for (size_t slot = 0; slot < MAX_QUEUED_PER_ENDPOINT; slot++) {
			if (!table.enqueue(ids[index], messageWith(MessageType::SYS_PING, FIRST_VALUE, NO_REPLY))) break;
			queued++;
		}
	}
	CHECK_EQ(QUEUED_MESSAGE_POOL, queued);
	CHECK_EQ(QUEUED_MESSAGE_POOL, table.queuedMessages());
	CHECK_FALSE(table.enqueue(ids[POOL_ENDPOINTS - LAST_INDEX_OFFSET],
							  messageWith(MessageType::SYS_PING, FIRST_VALUE, NO_REPLY)));

	Message out = {};
	CHECK_TRUE(table.dequeue(ids[0], out));
	CHECK_TRUE(table.enqueue(ids[POOL_ENDPOINTS - LAST_INDEX_OFFSET],
							 messageWith(MessageType::SYS_PING, SECOND_VALUE, NO_REPLY)));

	table.destroy(ids[1]);
	CHECK_TRUE(table.queuedMessages() < QUEUED_MESSAGE_POOL);
	table.clear();
	CHECK_EQ(NONE, table.queuedMessages());
	CHECK_FALSE(table.exists(FIRST_ENDPOINT));
}

void checkTakeServesTheEndpointsOfOneOwner() {
	EndpointTable table;
	uint32_t first = table.create();
	uint32_t second = table.create();
	CHECK_TRUE(table.bind(first, FIRST_OWNER));
	CHECK_TRUE(table.bind(second, SECOND_OWNER));
	CHECK_TRUE(table.enqueue(first, messageWith(MessageType::SYS_PING, FIRST_VALUE, NO_REPLY)));
	CHECK_TRUE(table.enqueue(second, messageWith(MessageType::SYS_PING, SECOND_VALUE, NO_REPLY)));

	Message out = {};
	CHECK_FALSE(table.hasQueuedFor(OTHER_OWNER));
	CHECK_FALSE(table.take(OTHER_OWNER, out));
	CHECK_FALSE(table.take(UNBOUND, out));
	CHECK_TRUE(table.hasQueuedFor(SECOND_OWNER));
	CHECK_TRUE(table.take(SECOND_OWNER, out));
	CHECK_EQ(SECOND_VALUE, out.payload.args.arg0);
	CHECK_FALSE(table.hasQueuedFor(SECOND_OWNER));
	CHECK_FALSE(table.take(SECOND_OWNER, out));
	CHECK_TRUE(table.take(FIRST_OWNER, out));
	CHECK_EQ(FIRST_VALUE, out.payload.args.arg0);
}

void checkMessagesSurviveTheOwnerButCapabilitiesDoNot() {
	EndpointTable table;
	uint32_t id = table.create();
	CHECK_TRUE(table.bind(id, FIRST_OWNER));
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::SYS_PING, FIRST_VALUE, FIRST_TOKEN)));
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::CAPABILITY_TRANSFER, SECOND_VALUE, SECOND_TOKEN)));
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::SYS_PING, THIRD_VALUE, NO_REPLY)));
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::CAPABILITY_TRANSFER, FOURTH_VALUE, NO_REPLY)));

	droppedCount = 0;
	table.releaseOwner(FIRST_OWNER, recordDrop);
	CHECK_EQ(DROPPED_TRANSFERS, droppedCount);
	CHECK_EQ(SECOND_TOKEN, droppedReplies[0]);
	CHECK_EQ(NO_REPLY, droppedReplies[1]);
	CHECK_EQ(UNBOUND, table.ownerOf(id));
	CHECK_EQ(KEPT_MESSAGES, table.queuedCount(id));

	Message out = {};
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::SYS_PING, FOURTH_VALUE, NO_REPLY)));
	CHECK_TRUE(table.bind(id, SECOND_OWNER, recordDrop));
	CHECK_EQ(DROPPED_TRANSFERS, droppedCount);
	CHECK_TRUE(table.take(SECOND_OWNER, out));
	CHECK_EQ(FIRST_VALUE, out.payload.args.arg0);
	CHECK_EQ(FIRST_TOKEN, out.reply);
	CHECK_TRUE(table.take(SECOND_OWNER, out));
	CHECK_EQ(THIRD_VALUE, out.payload.args.arg0);
	CHECK_TRUE(table.take(SECOND_OWNER, out));
	CHECK_EQ(FOURTH_VALUE, out.payload.args.arg0);
	CHECK_FALSE(table.take(SECOND_OWNER, out));
}

void checkAMessageAfterTheLastTransferIsStillLinked() {
	EndpointTable table;
	uint32_t id = table.create();
	CHECK_TRUE(table.bind(id, FIRST_OWNER));
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::SYS_PING, FIRST_VALUE, NO_REPLY)));
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::CAPABILITY_TRANSFER, SECOND_VALUE, NO_REPLY)));
	table.releaseOwner(FIRST_OWNER);
	CHECK_TRUE(table.enqueue(id, messageWith(MessageType::SYS_PING, THIRD_VALUE, NO_REPLY)));

	Message out = {};
	CHECK_TRUE(table.dequeue(id, out));
	CHECK_EQ(FIRST_VALUE, out.payload.args.arg0);
	CHECK_TRUE(table.dequeue(id, out));
	CHECK_EQ(THIRD_VALUE, out.payload.args.arg0);
	CHECK_FALSE(table.dequeue(id, out));
}

} // namespace

void testEndpointTable() {
	checkQueueKeepsOrderAndLimits();
	checkPoolIsSharedAndCellsAreReused();
	checkTakeServesTheEndpointsOfOneOwner();
	checkMessagesSurviveTheOwnerButCapabilitiesDoNot();
	checkAMessageAfterTheLastTransferIsStillLinked();
	checkIdsFollowInOrder();
	checkBindAndRelease();
	checkFullTableAndDestroy();
	checkHandleLookupIsByNodeAndType();
	checkFlagQueries();
	checkHandlesGiveNoGrantAuthority();
}

} // namespace host_test
