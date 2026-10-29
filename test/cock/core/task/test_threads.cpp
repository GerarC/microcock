#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/futex.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::task {

using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::Futex;
using core::task::Process;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using core::task::ThreadState;
using core::task::Timeouts;
using core::task::TimeoutState;

namespace {

constexpr uintptr_t FAKE_STACK_POINTER = 0x1000;
constexpr size_t DRAIN_LIMIT = 64;
constexpr uintptr_t FIRST_WORD = 0x40001000;
constexpr uintptr_t SECOND_WORD = 0x40001004;
constexpr uint64_t TIMEOUT_DELAY = 5;
constexpr size_t NOBODY = 0;
constexpr size_t ONE = 1;
constexpr size_t TWO = 2;
constexpr size_t THREE = 3;
constexpr size_t EVERYONE = 100;
constexpr uint32_t FIRST_THREAD_ID = 7;
constexpr uint32_t PARENT_ID = 42;
constexpr uint32_t EXIT_STATUS = 9;
constexpr uint32_t NO_BITS = 0;

using cock::abi::process::THREAD_BLOCKED;
using cock::abi::process::THREAD_WAITING_CALL;
using cock::abi::process::THREAD_WAITING_MSG;
using cock::abi::process::THREAD_WAITING_REPLY;

Thread *idleThread = nullptr;

ThreadResult entry() { return ThreadResult::success(); }

Thread *makeThread() { return new Thread(entry, ThreadPriority::NORMAL); }

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

Thread *blockedOn(uintptr_t address) {
	Thread *thread = makeThread();
	thread->setState(ThreadState::BLOCKED);
	Futex::enqueue(thread, address);
	return thread;
}

void checkWakeOrderAndKeys() {
	startScheduler();
	Process *process = TaskManager::getKernelProcess();
	Thread *first = blockedOn(FIRST_WORD);
	Thread *second = blockedOn(FIRST_WORD);
	Thread *third = blockedOn(FIRST_WORD);
	Thread *other = blockedOn(SECOND_WORD);
	EXPECT_EQ(static_cast<size_t>(4), Futex::waiterCount());

	EXPECT_EQ(TWO, Futex::wake(process, FIRST_WORD, TWO));
	EXPECT_TRUE(first->takeFutexWake());
	EXPECT_TRUE(second->takeFutexWake());
	EXPECT_FALSE(third->takeFutexWake());
	EXPECT_FALSE(other->takeFutexWake());
	EXPECT_TRUE(first->getState() == ThreadState::READY);
	EXPECT_TRUE(second->getState() == ThreadState::READY);
	EXPECT_TRUE(third->getState() == ThreadState::BLOCKED);
	EXPECT_FALSE(first->isFutexWaiting());
	EXPECT_EQ(TWO, Futex::waiterCount());

	EXPECT_EQ(ONE, Futex::wake(process, FIRST_WORD, EVERYONE));
	EXPECT_EQ(NOBODY, Futex::wake(process, FIRST_WORD, EVERYONE));
	EXPECT_EQ(ONE, Futex::wake(process, SECOND_WORD, ONE));
	EXPECT_EQ(NOBODY, Futex::waiterCount());

	drain();
	retire(first);
	retire(second);
	retire(third);
	retire(other);
	TaskManager::buryDeadThreads();
}

void checkWakeCancelsTimeout() {
	startScheduler();
	Process *process = TaskManager::getKernelProcess();
	Thread *thread = blockedOn(FIRST_WORD);
	Timeouts::arm(thread, TIMEOUT_DELAY);
	EXPECT_TRUE(thread->getTimeoutState() == TimeoutState::ARMED);

	EXPECT_EQ(ONE, Futex::wake(process, FIRST_WORD, ONE));
	EXPECT_TRUE(thread->getTimeoutState() == TimeoutState::NONE);
	EXPECT_EQ(NOBODY, Timeouts::armedCount());

	drain();
	retire(thread);
	TaskManager::buryDeadThreads();
}

void checkRemoveAndDeath() {
	startScheduler();
	Thread *removed = blockedOn(FIRST_WORD);
	Thread *dying = blockedOn(FIRST_WORD);
	EXPECT_EQ(TWO, Futex::waiterCount());

	Futex::remove(removed);
	EXPECT_FALSE(removed->isFutexWaiting());
	EXPECT_EQ(ONE, Futex::waiterCount());

	retire(dying);
	TaskManager::buryDeadThreads();
	EXPECT_EQ(NOBODY, Futex::waiterCount());

	drain();
	retire(removed);
	TaskManager::buryDeadThreads();
}

void checkThreadStateMask() {
	startScheduler();
	Process *process = TaskManager::getKernelProcess();
	uint32_t before = TaskManager::threadStateMask(*process);
	EXPECT_EQ(NO_BITS, before & (THREAD_WAITING_REPLY | THREAD_WAITING_CALL | THREAD_WAITING_MSG));

	Thread *replying = makeThread();
	Thread *accepting = makeThread();
	Thread *listening = makeThread();
	replying->setState(ThreadState::WAITING_REPLY);
	accepting->setState(ThreadState::WAITING_CALL);
	listening->setState(ThreadState::WAITING_MSG);

	uint32_t during = TaskManager::threadStateMask(*process);
	EXPECT_EQ(THREAD_WAITING_REPLY | THREAD_WAITING_CALL | THREAD_WAITING_MSG,
			  during & (THREAD_WAITING_REPLY | THREAD_WAITING_CALL | THREAD_WAITING_MSG));
	EXPECT_EQ(before & THREAD_BLOCKED, during & THREAD_BLOCKED);

	replying->setState(ThreadState::BLOCKED);
	EXPECT_EQ(THREAD_BLOCKED, TaskManager::threadStateMask(*process) & THREAD_BLOCKED);
	EXPECT_EQ(NO_BITS, TaskManager::threadStateMask(*process) & THREAD_WAITING_REPLY);

	replying->setState(ThreadState::DEAD);
	accepting->setState(ThreadState::DEAD);
	listening->setState(ThreadState::DEAD);
	uint32_t after = TaskManager::threadStateMask(*process);
	EXPECT_EQ(NO_BITS, after & (THREAD_BLOCKED | THREAD_WAITING_REPLY | THREAD_WAITING_CALL |
								THREAD_WAITING_MSG));

	TaskManager::notifyThreadDeath(replying);
	TaskManager::notifyThreadDeath(accepting);
	TaskManager::notifyThreadDeath(listening);
	TaskManager::buryDeadThreads();
}

void checkProcessCounters() {
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	EXPECT_FALSE(space.isNull());
	if (space.isNull()) return;

	Process *process =
		Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	EXPECT_EQ(core::task::Process::MAX_THREADS,
			  static_cast<size_t>(cock::abi::process::MAX_THREADS_PER_PROCESS));
	EXPECT_EQ(cock::abi::process::NO_PARENT, process->getParentId());
	EXPECT_FALSE(process->notifiesExit());

	process->setParentId(PARENT_ID);
	process->setNotifiesExit(true);
	process->setExitStatus(EXIT_STATUS);
	EXPECT_EQ(PARENT_ID, process->getParentId());
	EXPECT_TRUE(process->notifiesExit());
	EXPECT_EQ(EXIT_STATUS, process->getExitStatus());

	for (size_t thread = 0; thread < Process::MAX_THREADS; thread++) {
		EXPECT_TRUE(process->canAttachThread());
		process->attachThread(FIRST_THREAD_ID + static_cast<uint32_t>(thread));
	}
	EXPECT_FALSE(process->canAttachThread());
	EXPECT_EQ(FIRST_THREAD_ID, process->getId());
	EXPECT_EQ(Process::MAX_THREADS, process->getLiveThreadCount());

	for (size_t thread = 1; thread < Process::MAX_THREADS; thread++)
		EXPECT_FALSE(process->threadExited());
	EXPECT_TRUE(process->threadExited());
	EXPECT_EQ(NOBODY, process->getLiveThreadCount());
	EXPECT_EQ(Process::MAX_THREADS, process->getThreadCount());

	for (size_t thread = 0; thread < Process::MAX_THREADS; thread++)
		process->detachThread();
	delete process;
}

} // namespace

void test_threads() {
	TestRunner::begin_suite("Threads and futex");
	TaskManager::init();
	idleThread = new Thread(entry, ThreadPriority::IDLE);

	checkWakeOrderAndKeys();
	checkWakeCancelsTimeout();
	checkRemoveAndDeath();
	checkThreadStateMask();
	checkProcessCounters();

	Scheduler::init();
	retire(idleThread);
	TaskManager::buryDeadThreads();
	idleThread = nullptr;
	TestRunner::end_suite();
}

} // namespace cock::test::task
