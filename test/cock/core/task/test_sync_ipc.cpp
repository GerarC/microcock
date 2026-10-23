#include <cock/abi/call.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::task {

using abi::call::FAILURE;
using abi::call::SUCCESS;
using abi::call::WORD_COUNT;
using core::hal::SyscallFrame;
using core::task::IPC;
using core::task::AcceptResult;
using core::task::Scheduler;
using core::task::CallStart;
using core::task::SyncIpc;
using core::task::Timeouts;
using core::task::TimeoutState;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using core::task::ThreadState;
using ipc::Message;
using ipc::MessageType;

namespace {

constexpr uintptr_t FAKE_STACK_POINTER = 0x1000;
constexpr size_t DRAIN_LIMIT = 64;
constexpr uint32_t UNKNOWN_PID = 0xFFFFFFF0;
constexpr size_t PEER_VALUE = 0;
constexpr size_t FIRST_WORD_VALUE = 1;
constexpr uint32_t FIRST_REQUEST[WORD_COUNT] = {0x11, 0x12, 0x13, 0x14};
constexpr uint32_t SECOND_REQUEST[WORD_COUNT] = {0x21, 0x22, 0x23, 0x24};
constexpr uint32_t FIRST_REPLY[WORD_COUNT] = {0xA1, 0xA2, 0xA3, 0xA4};
constexpr uint32_t SECOND_REPLY[WORD_COUNT] = {0xB1, 0xB2, 0xB3, 0xB4};

constexpr uint32_t SHORT_WAIT_MILLISECONDS = 50;
constexpr uint32_t NO_WAIT_MILLISECONDS = 0;
constexpr uint64_t SETTLE_TICKS = 100;
constexpr uint32_t KEYBOARD_LINE = 1;

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

SyscallFrame frameOf(Thread *thread) {
	SyscallFrame frame = {};
	core::hal::read_syscall_frame(thread->getStackPointer(), frame);
	return frame;
}

bool hasWords(const SyscallFrame &frame, const uint32_t (&words)[WORD_COUNT]) {
	for (size_t index = 0; index < WORD_COUNT; index++)
		if (frame.values[FIRST_WORD_VALUE + index] != words[index]) return false;
	return true;
}

bool startCall(Thread &caller, uint32_t target, const uint32_t (&words)[WORD_COUNT],
			   uint32_t milliseconds = abi::call::NO_TIMEOUT) {
	return SyncIpc::call(caller, target, words, milliseconds) == CallStart::BLOCKED;
}

AcceptResult acceptNext(Thread &server, uint32_t milliseconds = abi::call::NO_TIMEOUT) {
	return SyncIpc::accept(server, milliseconds);
}

void pass(uint64_t ticks) {
	for (uint64_t tick = 0; tick < ticks; tick++)
		Timeouts::onTick();
}

void checkRefusals() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *dead = makeThread();
	dead->setState(ThreadState::DEAD);

	EXPECT_FALSE(startCall(*caller, UNKNOWN_PID, FIRST_REQUEST));
	EXPECT_FALSE(startCall(*caller, caller->getId(), FIRST_REQUEST));
	EXPECT_FALSE(startCall(*caller, dead->getId(), FIRST_REQUEST));
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	EXPECT_FALSE(SyncIpc::reply(*caller, FIRST_REPLY, false));

	Scheduler::takeRescheduleRequest();
	retire(caller);
	retire(dead);
	TaskManager::buryDeadThreads();
}

void checkQueuedCallThenAccept() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(startCall(*caller, server->getId(), FIRST_REQUEST));
	EXPECT_TRUE(caller->getState() == ThreadState::WAITING_REPLY);
	EXPECT_TRUE(SyncIpc::hasCallers(*server));
	EXPECT_TRUE(Scheduler::takeRescheduleRequest());

	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_FALSE(SyncIpc::hasCallers(*server));
	EXPECT_TRUE(server->serving == caller);
	SyscallFrame request = frameOf(server);
	EXPECT_EQ(SUCCESS, request.result);
	EXPECT_EQ(static_cast<uintptr_t>(caller->getId()), request.values[PEER_VALUE]);
	EXPECT_TRUE(hasWords(request, FIRST_REQUEST));
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::REFUSED);

	EXPECT_TRUE(SyncIpc::reply(*server, FIRST_REPLY, false));
	EXPECT_TRUE(server->serving == nullptr);
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	SyscallFrame answer = frameOf(caller);
	EXPECT_EQ(SUCCESS, answer.result);
	EXPECT_EQ(static_cast<uintptr_t>(server->getId()), answer.values[PEER_VALUE]);
	EXPECT_TRUE(hasWords(answer, FIRST_REPLY));
	EXPECT_FALSE(SyncIpc::reply(*server, FIRST_REPLY, false));

	drain();
	retire(caller);
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkWaitingServerGetsHandoff() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(acceptNext(*server) == AcceptResult::BLOCKED);
	EXPECT_TRUE(server->getState() == ThreadState::WAITING_CALL);
	Scheduler::takeRescheduleRequest();

	EXPECT_TRUE(startCall(*caller, server->getId(), SECOND_REQUEST));
	EXPECT_TRUE(server->getState() == ThreadState::READY);
	EXPECT_TRUE(server->serving == caller);
	EXPECT_TRUE(hasWords(frameOf(server), SECOND_REQUEST));
	EXPECT_TRUE(Scheduler::takeRescheduleRequest());

	EXPECT_TRUE(runNext() == server);
	EXPECT_TRUE(SyncIpc::reply(*server, SECOND_REPLY, true));
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	EXPECT_TRUE(Scheduler::takeRescheduleRequest());
	server->setState(ThreadState::WAITING_CALL);
	EXPECT_TRUE(runNext() == caller);
	EXPECT_TRUE(hasWords(frameOf(caller), SECOND_REPLY));

	drain();
	retire(caller);
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkFirstComeFirstServed() {
	startScheduler();
	Thread *first = makeThread();
	Thread *second = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(startCall(*first, server->getId(), FIRST_REQUEST));
	EXPECT_TRUE(startCall(*second, server->getId(), SECOND_REQUEST));
	Scheduler::takeRescheduleRequest();

	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(server->serving == first);
	EXPECT_TRUE(SyncIpc::reply(*server, FIRST_REPLY, false));
	EXPECT_TRUE(SyncIpc::hasCallers(*server));
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(server->serving == second);
	EXPECT_TRUE(hasWords(frameOf(server), SECOND_REQUEST));
	EXPECT_TRUE(SyncIpc::reply(*server, SECOND_REPLY, false));
	EXPECT_TRUE(hasWords(frameOf(second), SECOND_REPLY));

	drain();
	retire(first);
	retire(second);
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkDeathFailsCallers() {
	startScheduler();
	Thread *served = makeThread();
	Thread *queued = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(startCall(*served, server->getId(), FIRST_REQUEST));
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(startCall(*queued, server->getId(), SECOND_REQUEST));
	Scheduler::takeRescheduleRequest();

	retire(server);
	EXPECT_TRUE(served->getState() == ThreadState::READY);
	EXPECT_TRUE(queued->getState() == ThreadState::READY);
	EXPECT_EQ(FAILURE, frameOf(served).result);
	EXPECT_EQ(FAILURE, frameOf(queued).result);
	EXPECT_FALSE(SyncIpc::hasCallers(*server));
	EXPECT_TRUE(server->serving == nullptr);

	drain();
	retire(served);
	retire(queued);
	TaskManager::buryDeadThreads();
}

void checkCallTimeoutWhileQueued() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(SyncIpc::call(*caller, server->getId(), FIRST_REQUEST,
							  NO_WAIT_MILLISECONDS) == CallStart::TIMED_OUT);
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	EXPECT_FALSE(SyncIpc::hasCallers(*server));

	EXPECT_TRUE(startCall(*caller, server->getId(), FIRST_REQUEST, SHORT_WAIT_MILLISECONDS));
	EXPECT_TRUE(caller->getTimeoutState() == TimeoutState::ARMED);
	EXPECT_TRUE(SyncIpc::hasCallers(*server));
	Scheduler::takeRescheduleRequest();

	pass(SETTLE_TICKS);
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	EXPECT_EQ(abi::call::TIMED_OUT, frameOf(caller).result);
	EXPECT_FALSE(SyncIpc::hasCallers(*server));
	EXPECT_TRUE(caller->callTarget == nullptr);
	EXPECT_TRUE(acceptNext(*server, NO_WAIT_MILLISECONDS) == AcceptResult::TIMED_OUT);

	drain();
	retire(caller);
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkCallTimeoutWhileServed() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(startCall(*caller, server->getId(), FIRST_REQUEST, SHORT_WAIT_MILLISECONDS));
	Scheduler::takeRescheduleRequest();
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(server->serving == caller);

	pass(SETTLE_TICKS);
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	EXPECT_EQ(abi::call::TIMED_OUT, frameOf(caller).result);
	EXPECT_TRUE(server->serving == nullptr);
	EXPECT_FALSE(SyncIpc::reply(*server, FIRST_REPLY, false));

	drain();
	retire(caller);
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkReplyCancelsTheTimeout() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(startCall(*caller, server->getId(), FIRST_REQUEST, SHORT_WAIT_MILLISECONDS));
	Scheduler::takeRescheduleRequest();
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(SyncIpc::reply(*server, FIRST_REPLY, false));
	EXPECT_TRUE(caller->getTimeoutState() == TimeoutState::NONE);
	EXPECT_EQ(static_cast<size_t>(0), Timeouts::armedCount());

	pass(SETTLE_TICKS);
	EXPECT_EQ(SUCCESS, frameOf(caller).result);
	EXPECT_TRUE(hasWords(frameOf(caller), FIRST_REPLY));

	drain();
	retire(caller);
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkDeferThenReplyLater() {
	startScheduler();
	Thread *first = makeThread();
	Thread *second = makeThread();
	Thread *server = makeThread();
	Thread *other = makeThread();

	EXPECT_TRUE(SyncIpc::defer(*server) == nullptr);
	EXPECT_TRUE(startCall(*first, server->getId(), FIRST_REQUEST));
	Scheduler::takeRescheduleRequest();
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(SyncIpc::defer(*server) == first);
	EXPECT_TRUE(server->serving == nullptr);
	EXPECT_TRUE(first->getState() == ThreadState::WAITING_REPLY);
	EXPECT_FALSE(SyncIpc::reply(*server, FIRST_REPLY, false));

	EXPECT_TRUE(startCall(*second, server->getId(), SECOND_REQUEST));
	Scheduler::takeRescheduleRequest();
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(SyncIpc::reply(*server, SECOND_REPLY, false));
	EXPECT_TRUE(second->getState() == ThreadState::READY);
	EXPECT_TRUE(hasWords(frameOf(second), SECOND_REPLY));
	EXPECT_TRUE(first->getState() == ThreadState::WAITING_REPLY);

	EXPECT_FALSE(SyncIpc::replyTo(*server, UNKNOWN_PID, FIRST_REPLY));
	EXPECT_FALSE(SyncIpc::replyTo(*other, first->getId(), FIRST_REPLY));
	EXPECT_TRUE(first->getState() == ThreadState::WAITING_REPLY);

	EXPECT_TRUE(SyncIpc::replyTo(*server, first->getId(), FIRST_REPLY));
	EXPECT_TRUE(first->getState() == ThreadState::READY);
	EXPECT_EQ(SUCCESS, frameOf(first).result);
	EXPECT_EQ(static_cast<uintptr_t>(server->getId()), frameOf(first).values[PEER_VALUE]);
	EXPECT_TRUE(hasWords(frameOf(first), FIRST_REPLY));
	EXPECT_TRUE(first->callTarget == nullptr);
	EXPECT_TRUE(server->deferred == nullptr);
	EXPECT_FALSE(SyncIpc::replyTo(*server, first->getId(), FIRST_REPLY));

	drain();
	retire(first);
	retire(second);
	retire(server);
	retire(other);
	TaskManager::buryDeadThreads();
}

void checkDeferredCallTimesOut() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(startCall(*caller, server->getId(), FIRST_REQUEST, SHORT_WAIT_MILLISECONDS));
	Scheduler::takeRescheduleRequest();
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(SyncIpc::defer(*server) == caller);

	pass(SETTLE_TICKS);
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	EXPECT_EQ(abi::call::TIMED_OUT, frameOf(caller).result);
	EXPECT_TRUE(server->deferred == nullptr);
	EXPECT_FALSE(SyncIpc::replyTo(*server, caller->getId(), FIRST_REPLY));

	drain();
	retire(caller);
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkDeathFailsDeferredCalls() {
	startScheduler();
	Thread *caller = makeThread();
	Thread *server = makeThread();

	EXPECT_TRUE(startCall(*caller, server->getId(), FIRST_REQUEST));
	Scheduler::takeRescheduleRequest();
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::SERVED);
	EXPECT_TRUE(SyncIpc::defer(*server) == caller);

	retire(server);
	EXPECT_TRUE(caller->getState() == ThreadState::READY);
	EXPECT_EQ(FAILURE, frameOf(caller).result);
	EXPECT_TRUE(server->deferred == nullptr);

	drain();
	retire(caller);
	TaskManager::buryDeadThreads();
}

void finishServer(Thread *server) {
	drain();
	retire(server);
	TaskManager::buryDeadThreads();
}

void checkAcceptTakesPendingMessages() {
	startScheduler();
	Thread *server = makeThread();
	Message message = {};
	message.type = MessageType::SYS_PING;
	Message taken = {};

	EXPECT_TRUE(acceptNext(*server, NO_WAIT_MILLISECONDS) == AcceptResult::TIMED_OUT);
	EXPECT_TRUE(IPC::deliver(server->getId(), message));
	EXPECT_TRUE(acceptNext(*server) == AcceptResult::PENDING);
	EXPECT_TRUE(server->getNextMessage(taken));
	EXPECT_TRUE(server->getState() == ThreadState::READY);

	finishServer(server);
}

void checkMessageWakesAWaitingServer() {
	startScheduler();
	Thread *server = makeThread();
	Message message = {};
	message.type = MessageType::SYS_PING;
	Message taken = {};

	EXPECT_TRUE(acceptNext(*server, SHORT_WAIT_MILLISECONDS) == AcceptResult::BLOCKED);
	EXPECT_TRUE(server->getState() == ThreadState::WAITING_CALL);
	EXPECT_TRUE(server->getTimeoutState() == TimeoutState::ARMED);
	Scheduler::takeRescheduleRequest();

	EXPECT_TRUE(IPC::deliver(server->getId(), message));
	EXPECT_TRUE(server->getState() == ThreadState::READY);
	EXPECT_EQ(abi::call::MESSAGE_PENDING, frameOf(server).result);
	EXPECT_TRUE(server->getTimeoutState() == TimeoutState::NONE);
	EXPECT_EQ(static_cast<size_t>(0), Timeouts::armedCount());
	EXPECT_TRUE(server->getNextMessage(taken));

	finishServer(server);
}

void checkNotificationWakesAWaitingServer() {
	startScheduler();
	Thread *server = makeThread();

	EXPECT_TRUE(acceptNext(*server) == AcceptResult::BLOCKED);
	Scheduler::takeRescheduleRequest();
	EXPECT_TRUE(IPC::notify(server->getId(), KEYBOARD_LINE));
	EXPECT_TRUE(server->getState() == ThreadState::READY);
	EXPECT_EQ(abi::call::MESSAGE_PENDING, frameOf(server).result);
	uint32_t line = 0;
	EXPECT_TRUE(server->takeNotification(line));
	EXPECT_EQ(KEYBOARD_LINE, line);

	finishServer(server);
}

void checkAcceptTimesOut() {
	startScheduler();
	Thread *server = makeThread();

	EXPECT_TRUE(acceptNext(*server, SHORT_WAIT_MILLISECONDS) == AcceptResult::BLOCKED);
	Scheduler::takeRescheduleRequest();
	pass(SETTLE_TICKS);
	EXPECT_TRUE(server->getState() == ThreadState::READY);
	EXPECT_EQ(abi::call::TIMED_OUT, frameOf(server).result);
	EXPECT_TRUE(server->getTimeoutState() == TimeoutState::NONE);

	finishServer(server);
}

} // namespace

void test_sync_ipc() {
	TestRunner::begin_suite("Synchronous IPC");
	TaskManager::init();
	idleThread = new Thread(entry, ThreadPriority::IDLE);

	checkRefusals();
	checkQueuedCallThenAccept();
	checkWaitingServerGetsHandoff();
	checkFirstComeFirstServed();
	checkDeathFailsCallers();
	checkCallTimeoutWhileQueued();
	checkCallTimeoutWhileServed();
	checkReplyCancelsTheTimeout();
	checkDeferThenReplyLater();
	checkDeferredCallTimesOut();
	checkDeathFailsDeferredCalls();
	checkAcceptTakesPendingMessages();
	checkMessageWakesAWaitingServer();
	checkNotificationWakesAWaitingServer();
	checkAcceptTimesOut();

	Scheduler::init();
	retire(idleThread);
	TaskManager::buryDeadThreads();
	idleThread = nullptr;
	TestRunner::end_suite();
}

} // namespace cock::test::task
