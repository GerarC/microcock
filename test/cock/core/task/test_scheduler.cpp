#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <hal/cpu.hpp>
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
using ipc::Message;
using ipc::MessageType;

namespace {

constexpr uintptr_t FAKE_STACK_POINTER = 0x1000;
constexpr uint32_t BOOST_TICKS = 1000;
constexpr uint32_t QUANTUM_TICK_LIMIT = 64;
constexpr uint32_t LEVEL_ALLOWANCE_TICKS = 10;
constexpr uint32_t NO_TICKS = 0;
constexpr uint32_t CHARGED_TWICE = 2;
constexpr uint64_t SIMULATED_TICK_CYCLES = 400000;
constexpr uint32_t SHORTEST_DEMOTION_ROUNDS = 10;
constexpr uint32_t LONGEST_DEMOTION_ROUNDS = 32;
constexpr uint32_t SHORT_RUN_ROUNDS = 16;
constexpr size_t DRAIN_LIMIT = 64;
constexpr uint32_t UNKNOWN_PID = 0xFFFFFFF0;
constexpr uint32_t FIRST_ARGUMENT = 0xA1;
constexpr uint32_t SECOND_ARGUMENT = 0xB2;
constexpr uint32_t KEYBOARD_LINE = 1;
constexpr size_t INBOX_CAPACITY = core::task::THREAD_INBOX_CAPACITY;
constexpr size_t NO_MESSAGES = 0;

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

void checkPriorityOrder() {
	startScheduler();
	Thread *low = makeThread(ThreadPriority::LOW);
	Thread *normal = makeThread(ThreadPriority::NORMAL);
	Thread *high = makeThread(ThreadPriority::HIGH);
	Scheduler::addThread(low);
	Scheduler::addThread(normal);
	Scheduler::addThread(high);

	EXPECT_TRUE(runNext() == high);
	high->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == normal);
	normal->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == low);
	low->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == idleThread);

	drain();
	retire(low);
	retire(normal);
	retire(high);
	TaskManager::buryDeadThreads();
}

void checkRoundRobin() {
	startScheduler();
	Thread *first = makeThread(ThreadPriority::NORMAL);
	Thread *second = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(first);
	Scheduler::addThread(second);
	Scheduler::addThread(first);

	EXPECT_TRUE(runNext() == first);
	EXPECT_TRUE(runNext() == second);
	EXPECT_TRUE(runNext() == first);
	EXPECT_TRUE(first->getPriority() == ThreadPriority::NORMAL);
	EXPECT_TRUE(second->getPriority() == ThreadPriority::NORMAL);

	drain();
	retire(first);
	retire(second);
	TaskManager::buryDeadThreads();
}

void checkDemotion() {
	startScheduler();
	Thread *worker = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(worker);
	EXPECT_TRUE(runNext() == worker);

	uint32_t ticks = 0;
	while (worker->getPriority() == ThreadPriority::NORMAL &&
		   ticks < QUANTUM_TICK_LIMIT) {
		Scheduler::onTick(FAKE_STACK_POINTER);
		ticks++;
	}
	EXPECT_TRUE(worker->getPriority() == ThreadPriority::LOW);
	EXPECT_TRUE(Scheduler::getCurrentThread() == worker);

	drain();
	retire(worker);
	TaskManager::buryDeadThreads();
}

void checkYieldingEveryTickIsDemoted() {
	startScheduler();
	Thread *worker = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(worker);
	EXPECT_TRUE(runNext() == worker);

	uint32_t rounds = 0;
	while (worker->getPriority() == ThreadPriority::NORMAL &&
		   rounds < QUANTUM_TICK_LIMIT) {
		Scheduler::onTick(FAKE_STACK_POINTER);
		EXPECT_TRUE(runNext() == worker);
		rounds++;
	}
	EXPECT_TRUE(worker->getPriority() == ThreadPriority::LOW);
	EXPECT_EQ(LEVEL_ALLOWANCE_TICKS, rounds);
	EXPECT_EQ(NO_TICKS, worker->getLevelTicks());

	drain();
	retire(worker);
	TaskManager::buryDeadThreads();
}

void checkBlockingBeforeTheTickKeepsLevel() {
	startScheduler();
	Thread *worker = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(worker);

	for (uint32_t round = 0; round < QUANTUM_TICK_LIMIT; round++) {
		EXPECT_TRUE(runNext() == worker);
		worker->setState(ThreadState::BLOCKED);
		EXPECT_TRUE(runNext() == idleThread);
		worker->setState(ThreadState::READY);
		Scheduler::addThread(worker);
	}
	EXPECT_TRUE(runNext() == worker);
	EXPECT_TRUE(worker->getPriority() == ThreadPriority::NORMAL);
	EXPECT_EQ(NO_TICKS, worker->getLevelTicks());

	drain();
	retire(worker);
	TaskManager::buryDeadThreads();
}

void spinCycles(uint64_t cycles) {
	uint64_t start = core::hal::read_cycle_counter();
	while (core::hal::read_cycle_counter() - start < cycles) {
	}
}

void calibrateTick() {
	Scheduler::onTick(FAKE_STACK_POINTER);
	spinCycles(SIMULATED_TICK_CYCLES);
	Scheduler::onTick(FAKE_STACK_POINTER);
}

void checkStoppingBetweenTicksIsCharged() {
	startScheduler();
	calibrateTick();
	Thread *worker = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(worker);
	EXPECT_TRUE(runNext() == worker);

	uint32_t rounds = 0;
	while (worker->getPriority() == ThreadPriority::NORMAL &&
		   rounds < QUANTUM_TICK_LIMIT) {
		spinCycles(SIMULATED_TICK_CYCLES / 2 + SIMULATED_TICK_CYCLES / 10);
		EXPECT_TRUE(runNext() == worker);
		rounds++;
	}
	EXPECT_TRUE(worker->getPriority() == ThreadPriority::LOW);
	EXPECT_TRUE(rounds > SHORTEST_DEMOTION_ROUNDS);
	EXPECT_TRUE(rounds <= LONGEST_DEMOTION_ROUNDS);

	drain();
	retire(worker);
	TaskManager::buryDeadThreads();
}

void checkShortRunsAreNotCharged() {
	startScheduler();
	calibrateTick();
	Thread *worker = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(worker);

	for (uint32_t round = 0; round < SHORT_RUN_ROUNDS; round++) {
		EXPECT_TRUE(runNext() == worker);
		worker->setState(ThreadState::BLOCKED);
		EXPECT_TRUE(runNext() == idleThread);
		worker->setState(ThreadState::READY);
		Scheduler::addThread(worker);
	}
	EXPECT_TRUE(runNext() == worker);
	EXPECT_TRUE(worker->getPriority() == ThreadPriority::NORMAL);
	EXPECT_EQ(NO_TICKS, worker->getLevelTicks());

	drain();
	retire(worker);
	TaskManager::buryDeadThreads();
}

void checkNewLevelClearsTheCount() {
	Thread *worker = makeThread(ThreadPriority::NORMAL);
	worker->chargeLevelTick();
	worker->chargeLevelTick();
	EXPECT_EQ(CHARGED_TWICE, worker->getLevelTicks());

	worker->setPriority(ThreadPriority::HIGH);
	EXPECT_EQ(NO_TICKS, worker->getLevelTicks());

	retire(worker);
	TaskManager::buryDeadThreads();
}

void checkIdleIsNotDemoted() {
	startScheduler();
	EXPECT_TRUE(runNext() == idleThread);
	for (uint32_t tick = 0; tick < QUANTUM_TICK_LIMIT; tick++)
		Scheduler::onTick(FAKE_STACK_POINTER);
	EXPECT_TRUE(idleThread->getPriority() == ThreadPriority::IDLE);
	EXPECT_TRUE(Scheduler::getCurrentThread() == idleThread);
}

void checkBoost() {
	startScheduler();
	Thread *runner = makeThread(ThreadPriority::HIGH);
	Thread *waiting = makeThread(ThreadPriority::LOW);
	Scheduler::addThread(runner);
	Scheduler::addThread(waiting);
	EXPECT_TRUE(runNext() == runner);

	for (uint32_t tick = 0; tick < BOOST_TICKS - 1; tick++)
		Scheduler::onTick(FAKE_STACK_POINTER);
	EXPECT_FALSE(runner->getPriority() == ThreadPriority::CRITICAL);
	EXPECT_FALSE(waiting->getPriority() == ThreadPriority::CRITICAL);

	Scheduler::onTick(FAKE_STACK_POINTER);
	EXPECT_TRUE(runner->getPriority() == ThreadPriority::CRITICAL ||
				waiting->getPriority() == ThreadPriority::CRITICAL);

	drain();
	retire(runner);
	retire(waiting);
	TaskManager::buryDeadThreads();
}

void checkRequestFlags() {
	startScheduler();
	EXPECT_FALSE(Scheduler::takeRescheduleRequest());
	EXPECT_FALSE(Scheduler::takeRestartRequest());

	Scheduler::requestReschedule();
	Scheduler::requestRestart();
	EXPECT_TRUE(Scheduler::takeRescheduleRequest());
	EXPECT_FALSE(Scheduler::takeRescheduleRequest());
	EXPECT_TRUE(Scheduler::takeRestartRequest());
	EXPECT_FALSE(Scheduler::takeRestartRequest());

	Scheduler::requestReschedule();
	runNext();
	EXPECT_FALSE(Scheduler::takeRescheduleRequest());
}

void checkDeliver() {
	startScheduler();
	Thread *sender = makeThread(ThreadPriority::NORMAL);
	Thread *receiver = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(sender);
	EXPECT_TRUE(runNext() == sender);

	Message message = {};
	message.type = MessageType::SYS_PING;
	message.payload.args.arg0 = FIRST_ARGUMENT;
	EXPECT_TRUE(IPC::deliver(receiver->getId(), message));
	message.payload.args.arg0 = SECOND_ARGUMENT;
	EXPECT_TRUE(IPC::deliver(receiver->getId(), message));
	EXPECT_TRUE(receiver->hasMessages());

	Message received = {};
	EXPECT_TRUE(receiver->getNextMessage(received));
	EXPECT_EQ(receiver->getId(), received.target_pid);
	EXPECT_TRUE(received.type == MessageType::SYS_PING);
	EXPECT_EQ(FIRST_ARGUMENT, received.payload.args.arg0);
	EXPECT_TRUE(receiver->getNextMessage(received));
	EXPECT_EQ(SECOND_ARGUMENT, received.payload.args.arg0);
	EXPECT_FALSE(receiver->getNextMessage(received));
	EXPECT_FALSE(receiver->hasMessages());

	EXPECT_FALSE(IPC::deliver(UNKNOWN_PID, message));

	drain();
	retire(sender);
	retire(receiver);
	TaskManager::buryDeadThreads();
}

void checkDeliverToDeadThread() {
	startScheduler();
	Thread *target = makeThread(ThreadPriority::NORMAL);
	uint32_t pid = target->getId();
	target->setState(ThreadState::DEAD);

	Message message = {};
	message.type = MessageType::SYS_PING;
	EXPECT_FALSE(IPC::deliver(pid, message));
	EXPECT_FALSE(IPC::notify(pid, KEYBOARD_LINE));
	EXPECT_FALSE(target->hasMessages());
	EXPECT_FALSE(target->hasNotifications());

	retire(target);
	TaskManager::buryDeadThreads();
}

void checkFullInbox() {
	startScheduler();
	Thread *sender = makeThread(ThreadPriority::NORMAL);
	Thread *receiver = makeThread(ThreadPriority::NORMAL);
	Scheduler::addThread(sender);
	EXPECT_TRUE(runNext() == sender);

	Message message = {};
	message.type = MessageType::SYS_PING;
	size_t delivered = NO_MESSAGES;
	while (IPC::deliver(receiver->getId(), message) &&
		   delivered <= INBOX_CAPACITY)
		delivered++;
	EXPECT_EQ(INBOX_CAPACITY, delivered);
	EXPECT_FALSE(IPC::deliver(receiver->getId(), message));

	Message received = {};
	EXPECT_TRUE(receiver->getNextMessage(received));
	EXPECT_TRUE(IPC::deliver(receiver->getId(), message));

	drain();
	retire(sender);
	retire(receiver);
	TaskManager::buryDeadThreads();
}

void checkDeliverWakesWaitingThread() {
	startScheduler();
	Thread *sender = makeThread(ThreadPriority::NORMAL);
	Thread *receiver = makeThread(ThreadPriority::NORMAL);
	receiver->setState(ThreadState::WAITING_MSG);
	Scheduler::addThread(sender);
	EXPECT_TRUE(runNext() == sender);
	sender->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == idleThread);

	Scheduler::addThread(sender);
	sender->setState(ThreadState::READY);
	EXPECT_TRUE(runNext() == sender);

	Message message = {};
	message.type = MessageType::SYS_PING;
	EXPECT_TRUE(IPC::deliver(receiver->getId(), message));
	EXPECT_TRUE(receiver->getState() == ThreadState::READY);
	EXPECT_TRUE(IPC::deliver(receiver->getId(), message));

	sender->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == receiver);
	receiver->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == idleThread);

	drain();
	retire(sender);
	retire(receiver);
	TaskManager::buryDeadThreads();
}

void checkDeliverKeepsBlockedThreadBlocked() {
	startScheduler();
	Thread *sender = makeThread(ThreadPriority::NORMAL);
	Thread *receiver = makeThread(ThreadPriority::NORMAL);
	receiver->setState(ThreadState::BLOCKED);
	Scheduler::addThread(sender);
	EXPECT_TRUE(runNext() == sender);

	Message message = {};
	message.type = MessageType::SYS_PING;
	EXPECT_TRUE(IPC::deliver(receiver->getId(), message));
	EXPECT_TRUE(IPC::notify(receiver->getId(), KEYBOARD_LINE));
	EXPECT_TRUE(receiver->getState() == ThreadState::BLOCKED);

	sender->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == idleThread);

	drain();
	retire(sender);
	retire(receiver);
	TaskManager::buryDeadThreads();
}

void checkNotify() {
	startScheduler();
	Thread *target = makeThread(ThreadPriority::NORMAL);
	target->setState(ThreadState::WAITING_MSG);

	EXPECT_TRUE(IPC::notify(target->getId(), KEYBOARD_LINE));
	EXPECT_TRUE(IPC::notify(target->getId(), KEYBOARD_LINE));
	EXPECT_TRUE(target->hasNotifications());
	EXPECT_TRUE(target->getState() == ThreadState::READY);

	uint32_t line = 0;
	EXPECT_TRUE(target->takeNotification(line));
	EXPECT_EQ(KEYBOARD_LINE, line);
	EXPECT_FALSE(target->takeNotification(line));

	EXPECT_TRUE(runNext() == target);
	target->setState(ThreadState::BLOCKED);
	EXPECT_TRUE(runNext() == idleThread);

	drain();
	retire(target);
	TaskManager::buryDeadThreads();
}

} // namespace

void test_scheduler() {
	TestRunner::begin_suite("Scheduler and IPC");
	TaskManager::init();
	idleThread = makeThread(ThreadPriority::IDLE);

	checkPriorityOrder();
	checkRoundRobin();
	checkDemotion();
	checkYieldingEveryTickIsDemoted();
	checkBlockingBeforeTheTickKeepsLevel();
	checkNewLevelClearsTheCount();
	checkStoppingBetweenTicksIsCharged();
	checkShortRunsAreNotCharged();
	checkIdleIsNotDemoted();
	checkBoost();
	checkRequestFlags();
	checkDeliver();
	checkDeliverToDeadThread();
	checkFullInbox();
	checkDeliverWakesWaitingThread();
	checkDeliverKeepsBlockedThreadBlocked();
	checkNotify();

	Scheduler::init();
	retire(idleThread);
	TaskManager::buryDeadThreads();
	idleThread = nullptr;
	TestRunner::end_suite();
}

} // namespace cock::test::task
