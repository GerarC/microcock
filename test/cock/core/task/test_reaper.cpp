#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::task {

using cock::arch::x86::PhysicalMemoryManager;
using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::Process;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using core::task::ThreadState;

namespace {

constexpr uintptr_t FAKE_STACK_POINTER = 0x1000;
constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr size_t DRAIN_LIMIT = 16;
constexpr size_t WARM_UP_ROUNDS = 2;
constexpr size_t BATCH_THREADS = 4;

Thread *idleThread = nullptr;

ThreadResult entry() { return ThreadResult::success(); }

Thread *makeThread(ThreadPriority priority) {
	return new Thread(entry, priority);
}

void startScheduler() {
	Scheduler::init();
	Scheduler::setIdleThread(idleThread);
}

Thread *runNext() {
	Scheduler::onYield(FAKE_STACK_POINTER);
	return Scheduler::getCurrentThread();
}

void drain() {
	for (size_t round = 0; round < DRAIN_LIMIT; round++) {
		Thread *current = Scheduler::getCurrentThread();
		if (current == idleThread) return;
		if (current != nullptr) current->setState(ThreadState::BLOCKED);
		runNext();
	}
}

void retire(Thread *thread) {
	thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(thread);
}

Thread *makeUserThread() {
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	Process *process =
		Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	return new Thread(*process, VirtAddr(USER_ENTRY_POINT),
					  ThreadPriority::NORMAL);
}

void checkDeadThreadIsFreed() {
	EXPECT_TRUE(TaskManager::graveyardEmpty());

	Thread *thread = makeThread(ThreadPriority::NORMAL);
	uint32_t pid = thread->getId();
	EXPECT_TRUE(TaskManager::getThreadByPID(pid) == thread);

	retire(thread);
	EXPECT_FALSE(TaskManager::graveyardEmpty());
	EXPECT_TRUE(TaskManager::getThreadByPID(pid) == thread);

	TaskManager::buryDeadThreads();
	EXPECT_TRUE(TaskManager::graveyardEmpty());
	EXPECT_TRUE(TaskManager::getThreadByPID(pid) == nullptr);

	Thread *next = makeThread(ThreadPriority::NORMAL);
	EXPECT_EQ(TaskManager::slotOf(pid), TaskManager::slotOf(next->getId()));
	EXPECT_TRUE(pid != next->getId());
	EXPECT_TRUE(TaskManager::getThreadByPID(pid) == nullptr);
	EXPECT_TRUE(TaskManager::getThreadByPID(next->getId()) == next);
	retire(next);
	TaskManager::buryDeadThreads();
}

void checkBuryOnEmptyGraveyard() {
	EXPECT_TRUE(TaskManager::graveyardEmpty());
	TaskManager::buryDeadThreads();
	EXPECT_TRUE(TaskManager::graveyardEmpty());
}

void checkBatchIsFreed() {
	Thread *threads[BATCH_THREADS];
	uint32_t pids[BATCH_THREADS];
	for (size_t idx = 0; idx < BATCH_THREADS; idx++) {
		threads[idx] = makeThread(ThreadPriority::NORMAL);
		pids[idx] = threads[idx]->getId();
	}
	for (size_t idx = 0; idx < BATCH_THREADS; idx++) retire(threads[idx]);

	TaskManager::buryDeadThreads();
	EXPECT_TRUE(TaskManager::graveyardEmpty());
	for (size_t idx = 0; idx < BATCH_THREADS; idx++)
		EXPECT_TRUE(TaskManager::getThreadByPID(pids[idx]) == nullptr);
}

void runUserProcessCycle() {
	Thread *thread = makeUserThread();
	retire(thread);
	TaskManager::buryDeadThreads();
}

void checkProcessFramesReturn() {
	for (size_t round = 0; round < WARM_UP_ROUNDS; round++)
		runUserProcessCycle();

	size_t before = PhysicalMemoryManager::freeFrameCount();
	Thread *thread = makeUserThread();
	uint32_t pid = thread->getId();
	EXPECT_TRUE(PhysicalMemoryManager::freeFrameCount() < before);
	EXPECT_EQ(pid, thread->getProcess()->getId());

	retire(thread);
	TaskManager::buryDeadThreads();
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
	EXPECT_TRUE(TaskManager::getThreadByPID(pid) == nullptr);
}

void checkReaperWakesUp() {
	startScheduler();
	Thread *reaper = makeThread(ThreadPriority::NORMAL);
	reaper->setState(ThreadState::BLOCKED);
	TaskManager::setReaper(reaper);

	Thread *victim = makeThread(ThreadPriority::NORMAL);
	retire(victim);
	EXPECT_TRUE(reaper->getState() == ThreadState::READY);
	EXPECT_TRUE(runNext() == reaper);

	TaskManager::buryDeadThreads();
	EXPECT_TRUE(TaskManager::graveyardEmpty());
	reaper->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == idleThread);

	Thread *second = makeThread(ThreadPriority::NORMAL);
	retire(second);
	EXPECT_TRUE(reaper->getState() == ThreadState::READY);
	EXPECT_TRUE(runNext() == reaper);
	TaskManager::buryDeadThreads();

	drain();
	TaskManager::setReaper(nullptr);
	retire(reaper);
	TaskManager::buryDeadThreads();
}

void checkRunningReaperIsNotQueuedTwice() {
	startScheduler();
	Thread *reaper = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(reaper);
	TaskManager::setReaper(reaper);
	EXPECT_TRUE(runNext() == reaper);

	Thread *victim = makeThread(ThreadPriority::NORMAL);
	retire(victim);
	EXPECT_TRUE(reaper->getState() == ThreadState::RUNNING);
	TaskManager::buryDeadThreads();

	reaper->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == idleThread);

	drain();
	TaskManager::setReaper(nullptr);
	retire(reaper);
	TaskManager::buryDeadThreads();
}

} // namespace

void test_reaper() {
	TestRunner::begin_suite("Reaper");
	TaskManager::init();
	idleThread = makeThread(ThreadPriority::IDLE);

	checkDeadThreadIsFreed();
	checkBuryOnEmptyGraveyard();
	checkBatchIsFreed();
	checkProcessFramesReturn();
	checkReaperWakesUp();
	checkRunningReaperIsNotQueuedTwice();

	Scheduler::init();
	retire(idleThread);
	TaskManager::buryDeadThreads();
	idleThread = nullptr;
	TestRunner::end_suite();
}

} // namespace cock::test::task
