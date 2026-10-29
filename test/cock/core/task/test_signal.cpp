#include <cock/abi/process.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/ipc/message.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::task {

using cock::abi::process::KILLED_STATUS;
using cock::abi::process::SIGNAL_TERMINATE;
using cock::abi::process::THREAD_READY;
using cock::abi::process::THREAD_STOPPED;
using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::ExitNotice;
using core::task::IPC;
using core::task::Process;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadState;
using ipc::Message;
using ipc::MessageType;

namespace {

constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr size_t THREADS_IN_PROCESS = 1;
constexpr size_t NO_THREADS = 0;
constexpr uint32_t NO_BITS = 0;
constexpr uint32_t STATUS_BEFORE_KILL = 5;

Process *makeProcess() {
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	return Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
}

Thread *makeThread(Process &process) {
	return new Thread(process, VirtAddr(USER_ENTRY_POINT), ThreadPriority::NORMAL);
}

void retire(Thread *thread) {
	thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(thread);
}

void checkProcessEnds() {
	Process *victim = makeProcess();
	Thread *thread = makeThread(*victim);
	Thread *bystander = makeThread(*makeProcess());
	EXPECT_EQ(THREADS_IN_PROCESS, victim->getThreadCount());

	victim->setExitStatus(KILLED_STATUS);
	TaskManager::terminateProcess(*victim);

	EXPECT_TRUE(thread->getState() == ThreadState::DEAD);
	EXPECT_EQ(NO_THREADS, victim->getLiveThreadCount());
	EXPECT_EQ(KILLED_STATUS, victim->getExitStatus());
	EXPECT_TRUE(bystander->getState() != ThreadState::DEAD);

	TaskManager::terminateProcess(*victim);
	EXPECT_EQ(NO_THREADS, victim->getLiveThreadCount());

	TaskManager::buryDeadThreads();
	retire(bystander);
	TaskManager::buryDeadThreads();
}

void checkParentLearnsTheStatus() {
	Thread *parent = makeThread(*makeProcess());
	Process &parentProcess = *parent->getProcess();
	Thread *child = makeThread(*makeProcess());
	Process &childProcess = *child->getProcess();
	uint32_t childId = childProcess.getId();

	EXPECT_TRUE(parentProcess.reserveExitNotice());
	childProcess.setParentId(parentProcess.getId());
	childProcess.setNotifiesExit(true);
	childProcess.setExitStatus(STATUS_BEFORE_KILL);

	childProcess.setExitStatus(KILLED_STATUS);
	TaskManager::terminateProcess(childProcess);

	ExitNotice notice;
	EXPECT_TRUE(parent->takeExitNotice(notice));
	EXPECT_EQ(childId, notice.child);
	EXPECT_EQ(KILLED_STATUS, notice.status);
	EXPECT_FALSE(parent->hasExitNotices());

	TaskManager::buryDeadThreads();
	retire(parent);
	TaskManager::buryDeadThreads();
}

void checkSignalMessageReachesTheInbox() {
	Thread *target = makeThread(*makeProcess());
	EXPECT_FALSE(target->hasMessages());

	Message message = {};
	message.type = MessageType::SIGNAL;
	message.payload.args.arg0 = SIGNAL_TERMINATE;
	EXPECT_TRUE(IPC::deliver(target->getId(), message));
	EXPECT_TRUE(target->hasMessages());

	Message received = {};
	EXPECT_TRUE(target->getNextMessage(received));
	EXPECT_TRUE(received.type == MessageType::SIGNAL);
	EXPECT_EQ(SIGNAL_TERMINATE, received.payload.args.arg0);
	EXPECT_FALSE(target->hasMessages());

	retire(target);
	TaskManager::buryDeadThreads();
}

void checkStoppedProcessIsNotQueued() {
	Process *process = makeProcess();
	Thread *thread = makeThread(*process);
	Thread *bystander = makeThread(*makeProcess());
	Scheduler::addThread(thread);
	Scheduler::addThread(bystander);
	EXPECT_TRUE(thread->queued);
	EXPECT_EQ(NO_BITS, TaskManager::threadStateMask(*process) & THREAD_STOPPED);

	TaskManager::stopProcess(*process);
	EXPECT_TRUE(process->isStopped());
	EXPECT_FALSE(thread->queued);
	EXPECT_TRUE(bystander->queued);
	EXPECT_EQ(THREAD_STOPPED, TaskManager::threadStateMask(*process) & THREAD_STOPPED);
	EXPECT_EQ(THREAD_READY, TaskManager::threadStateMask(*process) & THREAD_READY);

	Scheduler::addThread(thread);
	EXPECT_FALSE(thread->queued);

	TaskManager::stopProcess(*process);
	EXPECT_FALSE(thread->queued);

	TaskManager::continueProcess(*process);
	EXPECT_FALSE(process->isStopped());
	EXPECT_TRUE(thread->queued);
	EXPECT_EQ(NO_BITS, TaskManager::threadStateMask(*process) & THREAD_STOPPED);

	TaskManager::continueProcess(*process);
	EXPECT_TRUE(thread->queued);

	TaskManager::stopProcess(*process);
	process->setExitStatus(KILLED_STATUS);
	TaskManager::terminateProcess(*process);
	EXPECT_TRUE(thread->getState() == ThreadState::DEAD);
	EXPECT_EQ(NO_THREADS, process->getLiveThreadCount());

	TaskManager::buryDeadThreads();
	Scheduler::terminateThread(bystander);
	TaskManager::buryDeadThreads();
}

void checkWokenThreadOfStoppedProcessWaitsForContinue() {
	Process *process = makeProcess();
	Thread *thread = makeThread(*process);
	thread->setState(ThreadState::WAITING_MSG);
	TaskManager::stopProcess(*process);

	Message message = {};
	message.type = MessageType::SIGNAL;
	message.payload.args.arg0 = SIGNAL_TERMINATE;
	EXPECT_TRUE(IPC::deliver(thread->getId(), message));
	EXPECT_TRUE(thread->getState() == ThreadState::READY);
	EXPECT_FALSE(thread->queued);

	TaskManager::continueProcess(*process);
	EXPECT_TRUE(thread->queued);

	Scheduler::terminateThread(thread);
	TaskManager::buryDeadThreads();
}

} // namespace

void test_signal() {
	TestRunner::begin_suite("Signals");
	TaskManager::init();

	checkProcessEnds();
	checkParentLearnsTheStatus();
	checkSignalMessageReachesTheInbox();
	checkStoppedProcessIsNotQueued();
	checkWokenThreadOfStoppedProcessWaitsForContinue();
	TestRunner::end_suite();
}

} // namespace cock::test::task
