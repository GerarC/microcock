#include <cock/abi/pm.hpp>
#include <cock/pm/table.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using cock::abi::pm::FIRST_FREE_PID;
using cock::abi::pm::INIT_PID;
using cock::abi::pm::NO_PID;
using cock::abi::pm::State;
using cock::abi::pm::Status;
using cock::pm::MAX_PROCESSES;
using cock::pm::Row;
using cock::pm::Table;

namespace {

constexpr uint32_t ROOT_KERNEL_ID = 1;
constexpr uint32_t FIRST_KERNEL_ID = 10;
constexpr uint32_t SECOND_KERNEL_ID = 11;
constexpr uint32_t THIRD_KERNEL_ID = 12;
constexpr uint32_t SOME_LABEL = 20;
constexpr uint32_t UNKNOWN_KERNEL_ID = 999;
constexpr uint32_t SOME_STATUS = 7;
constexpr size_t NO_ROWS = 0;
constexpr size_t ONE_ROW = 1;
constexpr size_t TWO_ROWS = 2;
constexpr size_t FIRST_INDEX = 0;
constexpr uint32_t HEADER_STATE_SHIFT = cock::abi::pm::BYTE_BITS;

void checkAdd() {
	Table table;
	CHECK_EQ(NO_ROWS, table.count());

	Row *root = table.add(ROOT_KERNEL_ID, NO_PID, false);
	CHECK_TRUE(root != nullptr);
	if (root == nullptr) return;
	CHECK_EQ(FIRST_FREE_PID, root->pid);
	CHECK_EQ(NO_PID, root->parent);
	CHECK_TRUE(root->state == State::RUNNING);

	Row *child = table.add(FIRST_KERNEL_ID, root->pid, true);
	CHECK_TRUE(child != nullptr);
	if (child == nullptr) return;
	CHECK_EQ(FIRST_FREE_PID + ONE_ROW, static_cast<size_t>(child->pid));
	CHECK_EQ(root->pid, child->parent);
	CHECK_TRUE(child->forward);
	CHECK_EQ(TWO_ROWS, table.count());

	CHECK_TRUE(table.add(FIRST_KERNEL_ID, NO_PID, false) == nullptr);
	CHECK_EQ(TWO_ROWS, table.count());
	CHECK_TRUE(table.add(cock::pm::NO_KERNEL_ID, NO_PID, false) == nullptr);

	CHECK_TRUE(table.findKernel(FIRST_KERNEL_ID) == child);
	CHECK_TRUE(table.find(child->pid) == child);
	CHECK_TRUE(table.findKernel(UNKNOWN_KERNEL_ID) == nullptr);
	CHECK_TRUE(table.find(NO_PID) == nullptr);
}

void checkInitPid() {
	Table table;
	Row *init = table.add(FIRST_KERNEL_ID, NO_PID, false, INIT_PID);
	CHECK_TRUE(init != nullptr);
	if (init == nullptr) return;
	CHECK_EQ(INIT_PID, init->pid);

	CHECK_TRUE(table.add(SECOND_KERNEL_ID, NO_PID, false, INIT_PID) == nullptr);

	Row *other = table.add(SECOND_KERNEL_ID, NO_PID, false);
	CHECK_TRUE(other != nullptr);
	if (other != nullptr) CHECK_EQ(FIRST_FREE_PID, other->pid);

	table.markExited(FIRST_KERNEL_ID, SOME_STATUS);
	Row *again = table.add(THIRD_KERNEL_ID, NO_PID, false, INIT_PID);
	CHECK_TRUE(again != nullptr);
	if (again != nullptr) CHECK_EQ(INIT_PID, again->pid);
	CHECK_TRUE(table.findKernel(FIRST_KERNEL_ID) == nullptr);
	CHECK_EQ(TWO_ROWS, table.count());
}

void checkAnnounce() {
	Table table;
	CHECK_TRUE(table.pendingAnnounce() == nullptr);

	Row *asked = table.add(FIRST_KERNEL_ID, NO_PID, false, NO_PID, SOME_LABEL);
	Row *plain = table.add(SECOND_KERNEL_ID, NO_PID, false);
	CHECK_TRUE(asked != nullptr && plain != nullptr);
	if (asked == nullptr || plain == nullptr) return;
	CHECK_EQ(SOME_LABEL, asked->label);
	CHECK_TRUE(table.pendingAnnounce() == asked);

	table.markExited(FIRST_KERNEL_ID, SOME_STATUS);
	asked->announcePending = false;
	CHECK_TRUE(table.pendingAnnounce() == nullptr);
}

void checkExit() {
	Table table;
	Row *kept = table.add(FIRST_KERNEL_ID, NO_PID, true);
	Row *plain = table.add(SECOND_KERNEL_ID, NO_PID, false);
	CHECK_TRUE(kept != nullptr && plain != nullptr);
	if (kept == nullptr || plain == nullptr) return;

	CHECK_TRUE(table.pendingForward() == nullptr);
	CHECK_TRUE(table.markExited(UNKNOWN_KERNEL_ID, SOME_STATUS) == nullptr);

	Row *exited = table.markExited(FIRST_KERNEL_ID, SOME_STATUS);
	CHECK_TRUE(exited == kept);
	CHECK_TRUE(kept->state == State::EXITED);
	CHECK_EQ(SOME_STATUS, kept->status);
	CHECK_TRUE(table.markExited(FIRST_KERNEL_ID, SOME_STATUS) == nullptr);
	CHECK_TRUE(table.pendingForward() == kept);

	kept->forwardPending = false;
	CHECK_TRUE(table.pendingForward() == nullptr);

	table.markExited(SECOND_KERNEL_ID, SOME_STATUS);
	CHECK_TRUE(table.pendingForward() == nullptr);
	CHECK_EQ(TWO_ROWS, table.count());
}

void checkFull() {
	Table table;
	for (size_t index = 0; index < MAX_PROCESSES; index++)
		CHECK_TRUE(table.add(FIRST_KERNEL_ID + static_cast<uint32_t>(index), NO_PID, false) != nullptr);
	CHECK_EQ(MAX_PROCESSES, table.count());

	uint32_t extra = FIRST_KERNEL_ID + static_cast<uint32_t>(MAX_PROCESSES);
	CHECK_TRUE(table.add(extra, NO_PID, false) == nullptr);

	table.markExited(FIRST_KERNEL_ID, SOME_STATUS);
	Row *reused = table.add(extra, NO_PID, false);
	CHECK_TRUE(reused != nullptr);
	CHECK_EQ(MAX_PROCESSES, table.count());
	CHECK_TRUE(table.findKernel(FIRST_KERNEL_ID) == nullptr);

	Table waiting;
	for (size_t index = 0; index < MAX_PROCESSES; index++)
		waiting.add(FIRST_KERNEL_ID + static_cast<uint32_t>(index), NO_PID, true);
	waiting.markExited(FIRST_KERNEL_ID, SOME_STATUS);
	CHECK_TRUE(waiting.add(extra, NO_PID, false) == nullptr);
}

void checkAt() {
	Table table;
	CHECK_TRUE(table.at(FIRST_INDEX) == nullptr);
	table.add(FIRST_KERNEL_ID, NO_PID, false);
	table.add(SECOND_KERNEL_ID, NO_PID, false);
	const Row *first = table.at(FIRST_INDEX);
	const Row *second = table.at(ONE_ROW);
	CHECK_TRUE(first != nullptr && second != nullptr);
	if (first != nullptr) CHECK_EQ(FIRST_KERNEL_ID, first->kernelId);
	if (second != nullptr) CHECK_EQ(SECOND_KERNEL_ID, second->kernelId);
	CHECK_TRUE(table.at(TWO_ROWS) == nullptr);

	table.clear();
	CHECK_EQ(NO_ROWS, table.count());
	CHECK_TRUE(table.at(FIRST_INDEX) == nullptr);
}

void checkHeaders() {
	CHECK_EQ(static_cast<uint32_t>(cock::abi::pm::Op::SPAWN),
			 cock::abi::pm::opOf(cock::abi::pm::pack(cock::abi::pm::Op::SPAWN)));

	uint32_t reply = cock::abi::pm::packReply(Status::NOT_FOUND, State::EXITED);
	CHECK_TRUE(cock::abi::pm::statusOf(reply) == Status::NOT_FOUND);
	CHECK_TRUE(cock::abi::pm::stateOf(reply) == State::EXITED);
	CHECK_EQ(static_cast<uint32_t>(State::EXITED), reply >> HEADER_STATE_SHIFT);
	CHECK_TRUE(cock::abi::pm::stateOf(cock::abi::pm::packReply(Status::OK)) == State::FREE);
}

} // namespace

void testPmTable() {
	checkAdd();
	checkInitPid();
	checkAnnounce();
	checkExit();
	checkFull();
	checkAt();
	checkHeaders();
}

} // namespace host_test
