#include <cock/abi/process.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/ipc/message.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::task {

using cock::abi::process::MAX_NOTIFIED_CHILDREN;
using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::ExitNotice;
using core::task::Process;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadState;
using ipc::Message;

namespace {

constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr uint32_t FIRST_CHILD = 0;
constexpr uint32_t STATUS_BASE = 100;
constexpr uint32_t SINGLE_CHILD = 1;

Thread *makeUserThread() {
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	Process *process =
		Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	return new Thread(*process, VirtAddr(USER_ENTRY_POINT), ThreadPriority::NORMAL);
}

void retire(Thread *thread) {
	thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(thread);
}

void fillInbox(Thread &thread) {
	Message message = {};
	while (thread.receiveMessage(message)) {
	}
}

void checkReservationIsBounded() {
	Thread *parent = makeUserThread();
	Process &process = *parent->getProcess();

	for (size_t child = 0; child < MAX_NOTIFIED_CHILDREN; child++) {
		EXPECT_TRUE(process.canReserveExitNotice());
		EXPECT_TRUE(process.reserveExitNotice());
	}
	EXPECT_FALSE(process.canReserveExitNotice());
	EXPECT_FALSE(process.reserveExitNotice());
	EXPECT_FALSE(process.hasExitNotices());

	for (size_t child = 0; child < MAX_NOTIFIED_CHILDREN; child++)
		EXPECT_TRUE(process.recordExitNotice(static_cast<uint32_t>(child), STATUS_BASE));
	EXPECT_FALSE(process.canReserveExitNotice());

	ExitNotice notice;
	EXPECT_TRUE(process.takeExitNotice(notice));
	EXPECT_EQ(FIRST_CHILD, notice.child);
	EXPECT_TRUE(process.canReserveExitNotice());

	while (process.takeExitNotice(notice)) {
	}
	EXPECT_FALSE(process.hasExitNotices());
	EXPECT_TRUE(process.canReserveExitNotice());

	retire(parent);
	TaskManager::buryDeadThreads();
}

void checkFullInboxDoesNotLoseNotices() {
	Thread *parent = makeUserThread();
	Process &process = *parent->getProcess();
	fillInbox(*parent);

	Thread *children[MAX_NOTIFIED_CHILDREN];
	uint32_t ids[MAX_NOTIFIED_CHILDREN];
	for (size_t idx = 0; idx < MAX_NOTIFIED_CHILDREN; idx++) {
		EXPECT_TRUE(process.reserveExitNotice());
		children[idx] = makeUserThread();
		Process &child = *children[idx]->getProcess();
		child.setParentId(process.getId());
		child.setNotifiesExit(true);
		child.setExitStatus(STATUS_BASE + static_cast<uint32_t>(idx));
		ids[idx] = child.getId();
	}
	EXPECT_FALSE(process.canReserveExitNotice());

	for (size_t idx = 0; idx < MAX_NOTIFIED_CHILDREN; idx++) retire(children[idx]);

	EXPECT_TRUE(parent->hasExitNotices());
	for (size_t idx = 0; idx < MAX_NOTIFIED_CHILDREN; idx++) {
		ExitNotice notice;
		EXPECT_TRUE(parent->takeExitNotice(notice));
		EXPECT_EQ(ids[idx], notice.child);
		EXPECT_EQ(STATUS_BASE + static_cast<uint32_t>(idx), notice.status);
	}
	EXPECT_FALSE(parent->hasExitNotices());
	EXPECT_TRUE(process.canReserveExitNotice());
	EXPECT_TRUE(parent->hasMessages());

	TaskManager::buryDeadThreads();
	retire(parent);
	TaskManager::buryDeadThreads();
}

void checkMainThreadReadsNotices() {
	Thread *parent = makeUserThread();
	Process &process = *parent->getProcess();
	EXPECT_TRUE(parent->isMainThread());
	EXPECT_TRUE(process.reserveExitNotice());
	EXPECT_TRUE(process.recordExitNotice(SINGLE_CHILD, STATUS_BASE));
	EXPECT_TRUE(parent->hasExitNotices());

	ExitNotice notice;
	EXPECT_TRUE(parent->takeExitNotice(notice));
	EXPECT_EQ(SINGLE_CHILD, notice.child);
	EXPECT_EQ(STATUS_BASE, notice.status);

	retire(parent);
	TaskManager::buryDeadThreads();
}

} // namespace

void test_exit_notice() {
	TestRunner::begin_suite("Exit notices");
	TaskManager::init();

	checkReservationIsBounded();
	checkFullInboxDoesNotLoseNotices();
	checkMainThreadReadsNotices();
	TestRunner::end_suite();
}

} // namespace cock::test::task
