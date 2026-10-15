#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <cock/ipc/message.hpp>
#include <hal/cpu.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::task {

using core::task::IPC;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using core::task::ThreadState;
using core::task::TimeoutPhase;
using core::task::Timeouts;
using core::task::TimeoutState;
using ipc::Message;
using ipc::MessageType;

namespace {

constexpr uintptr_t FAKE_STACK_POINTER = 0x1000;
constexpr size_t DRAIN_LIMIT = 64;
constexpr uint64_t SHORT_DELAY = 3;
constexpr uint64_t LONG_DELAY = 7;
constexpr uint64_t EXTRA_TICKS = 20;
constexpr uint32_t ONE_MILLISECOND = 1;
constexpr uint32_t ONE_TICK_MILLISECONDS = 10;
constexpr uint32_t JUST_OVER_ONE_TICK_MILLISECONDS = 11;
constexpr uint32_t ONE_SECOND_MILLISECONDS = 1000;
constexpr uint32_t NO_MILLISECONDS = 0;
constexpr uint32_t WAIT_MILLISECONDS = 50;
constexpr uint64_t NO_TICKS = 0;
constexpr size_t NO_ARMED = 0;
constexpr size_t ONE_ARMED = 1;
constexpr size_t TWO_ARMED = 2;

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

void pass(uint64_t ticks) {
	for (uint64_t tick = 0; tick < ticks; tick++)
		Timeouts::onTick();
}

void checkConversion() {
	EXPECT_EQ(NO_TICKS, Timeouts::ticksFor(NO_MILLISECONDS));
	EXPECT_EQ(static_cast<uint64_t>(1), Timeouts::ticksFor(ONE_MILLISECOND));
	EXPECT_EQ(static_cast<uint64_t>(1), Timeouts::ticksFor(ONE_TICK_MILLISECONDS));
	EXPECT_EQ(static_cast<uint64_t>(2),
			  Timeouts::ticksFor(JUST_OVER_ONE_TICK_MILLISECONDS));
	EXPECT_EQ(static_cast<uint64_t>(core::hal::TIMER_FREQUENCY_HZ),
			  Timeouts::ticksFor(ONE_SECOND_MILLISECONDS));
}

void checkWakeOrder() {
	startScheduler();
	Thread *waiting = makeThread();
	Thread *sleeping = makeThread();
	waiting->setState(ThreadState::WAITING_MSG);
	sleeping->setState(ThreadState::BLOCKED);
	Timeouts::arm(waiting, SHORT_DELAY);
	Timeouts::arm(sleeping, LONG_DELAY);
	EXPECT_EQ(TWO_ARMED, Timeouts::armedCount());

	pass(SHORT_DELAY + Timeouts::TICK_MARGIN - 1);
	EXPECT_TRUE(waiting->getState() == ThreadState::WAITING_MSG);
	EXPECT_TRUE(waiting->getTimeoutState() == TimeoutState::ARMED);

	pass(1);
	EXPECT_TRUE(waiting->getState() == ThreadState::READY);
	EXPECT_TRUE(waiting->getTimeoutState() == TimeoutState::EXPIRED);
	EXPECT_TRUE(sleeping->getState() == ThreadState::BLOCKED);
	EXPECT_EQ(ONE_ARMED, Timeouts::armedCount());

	pass(LONG_DELAY - SHORT_DELAY);
	EXPECT_TRUE(sleeping->getState() == ThreadState::READY);
	EXPECT_TRUE(sleeping->getTimeoutState() == TimeoutState::EXPIRED);
	EXPECT_EQ(NO_ARMED, Timeouts::armedCount());

	Timeouts::cancel(waiting);
	Timeouts::cancel(sleeping);
	drain();
	retire(waiting);
	retire(sleeping);
	TaskManager::buryDeadThreads();
}

void checkWaitPhases() {
	startScheduler();
	Thread *thread = makeThread();

	EXPECT_TRUE(Timeouts::wait(*thread, NO_MILLISECONDS,
							   ThreadState::WAITING_MSG) ==
				TimeoutPhase::TIMED_OUT);
	EXPECT_TRUE(thread->getTimeoutState() == TimeoutState::NONE);

	EXPECT_TRUE(Timeouts::wait(*thread, WAIT_MILLISECONDS,
							   ThreadState::WAITING_MSG) == TimeoutPhase::WAITING);
	EXPECT_TRUE(thread->getState() == ThreadState::WAITING_MSG);
	EXPECT_TRUE(thread->getTimeoutState() == TimeoutState::ARMED);
	EXPECT_TRUE(Scheduler::takeRescheduleRequest());
	EXPECT_TRUE(Scheduler::takeRestartRequest());

	EXPECT_TRUE(Timeouts::wait(*thread, WAIT_MILLISECONDS,
							   ThreadState::WAITING_MSG) == TimeoutPhase::WAITING);
	EXPECT_EQ(ONE_ARMED, Timeouts::armedCount());
	Scheduler::takeRescheduleRequest();
	Scheduler::takeRestartRequest();

	pass(Timeouts::ticksFor(WAIT_MILLISECONDS) + Timeouts::TICK_MARGIN);
	EXPECT_TRUE(thread->getState() == ThreadState::READY);
	EXPECT_TRUE(Timeouts::wait(*thread, WAIT_MILLISECONDS,
							   ThreadState::WAITING_MSG) ==
				TimeoutPhase::TIMED_OUT);
	EXPECT_TRUE(thread->getTimeoutState() == TimeoutState::NONE);
	EXPECT_FALSE(Scheduler::takeRescheduleRequest());

	drain();
	retire(thread);
	TaskManager::buryDeadThreads();
}

void checkMessageCancels() {
	startScheduler();
	Thread *thread = makeThread();
	thread->setState(ThreadState::WAITING_MSG);
	Timeouts::arm(thread, SHORT_DELAY);

	Message message = {};
	message.type = MessageType::SYS_PING;
	EXPECT_TRUE(IPC::deliver(thread->getId(), message));
	EXPECT_TRUE(thread->getState() == ThreadState::READY);

	Timeouts::cancel(thread);
	EXPECT_TRUE(thread->getTimeoutState() == TimeoutState::NONE);
	EXPECT_EQ(NO_ARMED, Timeouts::armedCount());

	EXPECT_TRUE(runNext() == thread);
	thread->setState(ThreadState::BLOCKED);
	pass(EXTRA_TICKS);
	EXPECT_TRUE(thread->getState() == ThreadState::BLOCKED);
	EXPECT_TRUE(thread->getTimeoutState() == TimeoutState::NONE);

	drain();
	retire(thread);
	TaskManager::buryDeadThreads();
}

void checkDeathDisarms() {
	startScheduler();
	Thread *thread = makeThread();
	Thread *survivor = makeThread();
	survivor->setState(ThreadState::BLOCKED);
	thread->setState(ThreadState::BLOCKED);
	Timeouts::arm(thread, SHORT_DELAY);
	Timeouts::arm(survivor, LONG_DELAY);
	EXPECT_EQ(TWO_ARMED, Timeouts::armedCount());

	retire(thread);
	TaskManager::buryDeadThreads();
	EXPECT_EQ(ONE_ARMED, Timeouts::armedCount());

	pass(LONG_DELAY + Timeouts::TICK_MARGIN);
	EXPECT_TRUE(survivor->getState() == ThreadState::READY);
	EXPECT_EQ(NO_ARMED, Timeouts::armedCount());

	Timeouts::cancel(survivor);
	drain();
	retire(survivor);
	TaskManager::buryDeadThreads();
}

} // namespace

void test_timeouts() {
	TestRunner::begin_suite("Timeouts");
	TaskManager::init();
	idleThread = new Thread(entry, ThreadPriority::IDLE);

	checkConversion();
	checkWakeOrder();
	checkWaitPhases();
	checkMessageCancels();
	checkDeathDisarms();

	Scheduler::init();
	retire(idleThread);
	TaskManager::buryDeadThreads();
	idleThread = nullptr;
	TestRunner::end_suite();
}

} // namespace cock::test::task
