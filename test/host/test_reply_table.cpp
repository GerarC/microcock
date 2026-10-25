#include <cock/core/task/reply_table.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using cock::abi::endpoint::MAX_REPLIES_PER_REQUESTER;
using cock::abi::endpoint::NO_REPLY;
using cock::core::task::ReplyTable;

namespace {

constexpr uint32_t SERVER = 4;
constexpr uint32_t OTHER_SERVER = 5;
constexpr uint32_t CLIENT = 8;
constexpr uint32_t CLIENT_THREAD = 9;
constexpr uint32_t OTHER_CLIENT = 12;
constexpr uint32_t OTHER_CLIENT_THREAD = 13;
constexpr uint32_t NO_ID = 0;
constexpr uint32_t NEXT_TOKEN_OFFSET = 1;
constexpr uint32_t REUSES = 1000;

void checkOpenAndClose() {
	ReplyTable table;
	uint32_t token = table.open(SERVER, CLIENT_THREAD, CLIENT);
	CHECK_TRUE(token != NO_REPLY);
	CHECK_TRUE(table.holds(token, SERVER));
	CHECK_FALSE(table.holds(token, CLIENT));
	CHECK_EQ(CLIENT_THREAD, table.requesterOf(token));
	CHECK_EQ(static_cast<size_t>(1), table.openCount());

	CHECK_TRUE(table.close(token));
	CHECK_FALSE(table.holds(token, SERVER));
	CHECK_FALSE(table.close(token));
	CHECK_EQ(NO_ID, table.requesterOf(token));
	CHECK_EQ(static_cast<size_t>(0), table.openCount());
}

void checkZeroIdsAndForgedTokens() {
	ReplyTable table;
	CHECK_EQ(NO_REPLY, table.open(NO_ID, CLIENT_THREAD, CLIENT));
	CHECK_EQ(NO_REPLY, table.open(SERVER, NO_ID, CLIENT));
	CHECK_EQ(NO_REPLY, table.open(SERVER, CLIENT_THREAD, NO_ID));

	uint32_t token = table.open(SERVER, CLIENT_THREAD, CLIENT);
	CHECK_FALSE(table.holds(NO_REPLY, SERVER));
	CHECK_FALSE(table.holds(token + NEXT_TOKEN_OFFSET, SERVER));
	CHECK_FALSE(table.close(NO_REPLY));
}

void checkStaleTokenNeverWorksAgain() {
	ReplyTable table;
	uint32_t first = table.open(SERVER, CLIENT_THREAD, CLIENT);
	CHECK_TRUE(table.close(first));

	uint32_t second = table.open(SERVER, CLIENT_THREAD, CLIENT);
	CHECK_TRUE(second != first);
	CHECK_FALSE(table.holds(first, SERVER));
	CHECK_TRUE(table.holds(second, SERVER));

	for (uint32_t round = 0; round < REUSES; round++) {
		table.close(second);
		uint32_t next = table.open(SERVER, CLIENT_THREAD, CLIENT);
		CHECK_TRUE(next != NO_REPLY && next != second);
		second = next;
	}
	CHECK_FALSE(table.holds(first, SERVER));
}

void checkPassMovesTheHolder() {
	ReplyTable table;
	uint32_t token = table.open(SERVER, CLIENT_THREAD, CLIENT);
	CHECK_FALSE(table.pass(token, OTHER_SERVER, SERVER));
	CHECK_FALSE(table.pass(token, SERVER, NO_ID));
	CHECK_TRUE(table.pass(token, SERVER, OTHER_SERVER));
	CHECK_FALSE(table.holds(token, SERVER));
	CHECK_TRUE(table.holds(token, OTHER_SERVER));
	CHECK_FALSE(table.pass(token, SERVER, OTHER_SERVER));
	CHECK_EQ(CLIENT_THREAD, table.requesterOf(token));
}

void checkOneRequesterCannotUseUpTheTable() {
	ReplyTable table;
	for (size_t index = 0; index < MAX_REPLIES_PER_REQUESTER; index++)
		CHECK_TRUE(table.open(SERVER, CLIENT_THREAD, CLIENT) != NO_REPLY);
	CHECK_EQ(NO_REPLY, table.open(SERVER, CLIENT_THREAD, CLIENT));
	CHECK_TRUE(table.open(SERVER, OTHER_CLIENT_THREAD, OTHER_CLIENT) != NO_REPLY);
}

void checkTableFills() {
	ReplyTable table;
	uint32_t client = CLIENT;
	size_t opened = 0;
	for (;;) {
		bool made = false;
		for (size_t index = 0; index < MAX_REPLIES_PER_REQUESTER; index++) {
			if (table.open(SERVER, client + NEXT_TOKEN_OFFSET, client) == NO_REPLY) break;
			opened++;
			made = true;
		}
		if (!made) break;
		client += OTHER_CLIENT - CLIENT;
	}
	CHECK_EQ(ReplyTable::MAX_REPLIES, opened);
	CHECK_EQ(NO_REPLY, table.open(SERVER, CLIENT_THREAD, CLIENT + NEXT_TOKEN_OFFSET));
	table.clear();
	CHECK_EQ(static_cast<size_t>(0), table.openCount());
}

void checkEndingProcessDropsItsTokens() {
	ReplyTable table;
	uint32_t held = table.open(SERVER, CLIENT_THREAD, CLIENT);
	uint32_t asked = table.open(OTHER_SERVER, OTHER_CLIENT_THREAD, OTHER_CLIENT);
	uint32_t unrelated = table.open(OTHER_SERVER, CLIENT_THREAD, CLIENT);

	table.releaseProcess(SERVER);
	CHECK_FALSE(table.holds(held, SERVER));
	CHECK_TRUE(table.holds(asked, OTHER_SERVER));

	table.releaseProcess(OTHER_CLIENT);
	CHECK_FALSE(table.holds(asked, OTHER_SERVER));
	CHECK_TRUE(table.holds(unrelated, OTHER_SERVER));

	table.releaseProcess(NO_ID);
	CHECK_EQ(static_cast<size_t>(1), table.openCount());
}

} // namespace

void testReplyTable() {
	checkOpenAndClose();
	checkZeroIdsAndForgedTokens();
	checkStaleTokenNeverWorksAgain();
	checkPassMovesTheHolder();
	checkOneRequesterCannotUseUpTheTable();
	checkTableFills();
	checkEndingProcessDropsItsTokens();
}

} // namespace host_test
