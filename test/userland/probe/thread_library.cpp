#include "probe.hpp"
#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <cock/user/condition_variable.hpp>
#include <cock/user/joinable_thread.hpp>
#include <cock/user/mutex.hpp>
#include <cock/user/semaphore.hpp>
#include <stddef.h>
#include <stdint.h>

using cock::user::ConditionVariable;
using cock::user::JoinableThread;
using cock::user::Mutex;
using cock::user::Semaphore;

namespace probe {

namespace {

constexpr size_t STACK_BYTES = 0x4000;
constexpr size_t WAITER_COUNT = 3;
constexpr size_t PLAYER_COUNT = 2;
constexpr size_t FIRST_STACK = 0;
constexpr uint32_t WORK_MILLISECONDS = 30;
constexpr uint32_t EARLY_JOIN_MILLISECONDS = 10;
constexpr uint32_t SETTLE_MILLISECONDS = 50;
constexpr uint32_t DEADLINE_MILLISECONDS = 3000;
constexpr uint32_t JOIN_DONE_VALUE = 0x600D;
constexpr uint32_t NOT_DONE = 0;
constexpr uint32_t TURN_ROUNDS = 50;
constexpr uintptr_t FIRST_PLAYER = 0;
constexpr uintptr_t SECOND_PLAYER = 1;
constexpr uint32_t NO_COUNT = 0;
constexpr uint32_t ONE_TOKEN = 1;
constexpr uint32_t TWO_TOKENS = 2;
constexpr uint32_t INITIAL_UNITS = 2;
constexpr uint32_t ONE_UNIT = 1;
constexpr uint32_t ADD_ONE = 1;
constexpr uint32_t HANDOFF_ROUNDS = 20;
constexpr size_t WAITER_THREAD = 0;

struct SignalingMutex {
	Mutex inner;
	ConditionVariable *signalOnUnlock = nullptr;

	void lock() { inner.lock(); }

	void unlock() {
		inner.unlock();
		if (signalOnUnlock != nullptr) signalOnUnlock->signal();
	}
};

alignas(PAGE_BYTES) uint8_t stacks[WAITER_COUNT][STACK_BYTES];

JoinableThread joinThread;
JoinableThread players[PLAYER_COUNT];
JoinableThread waiters[WAITER_COUNT];

uint32_t joinValue = NOT_DONE;

Mutex turnLock;
ConditionVariable turnChanged;
uint32_t turn = 0;
uint32_t turnsTaken = 0;

SignalingMutex handoffLock;
ConditionVariable handoffChanged;

Mutex tokenLock;
ConditionVariable tokenChanged;
uint32_t tokens = 0;
uint32_t tokensTaken = 0;

Semaphore gate;
uint32_t passed = 0;

void slowWorker(void *) {
	cock::user::sleep_ms(WORK_MILLISECONDS);
	__atomic_store_n(&joinValue, JOIN_DONE_VALUE, __ATOMIC_SEQ_CST);
}

void quickWorker(void *) {}

void player(void *self) {
	uintptr_t me = reinterpret_cast<uintptr_t>(self);
	uintptr_t other = me == FIRST_PLAYER ? SECOND_PLAYER : FIRST_PLAYER;
	for (uint32_t round = 0; round < TURN_ROUNDS; round++) {
		turnLock.lock();
		while (turn != me) turnChanged.wait(turnLock);
		turnsTaken = turnsTaken + ADD_ONE;
		turn = static_cast<uint32_t>(other);
		turnChanged.broadcast();
		turnLock.unlock();
	}
}

void handoffWaiter(void *) {
	for (uint32_t round = 0; round < HANDOFF_ROUNDS; round++) {
		handoffLock.lock();
		handoffChanged.wait(handoffLock);
		handoffLock.unlock();
	}
}

void tokenWaiter(void *) {
	tokenLock.lock();
	while (tokens == NO_COUNT) tokenChanged.wait(tokenLock);
	tokens = tokens - ONE_TOKEN;
	tokensTaken = tokensTaken + ADD_ONE;
	tokenLock.unlock();
}

void gateWaiter(void *) {
	gate.wait();
	__atomic_add_fetch(&passed, ADD_ONE, __ATOMIC_SEQ_CST);
}

uint32_t readTokensTaken() {
	tokenLock.lock();
	uint32_t seen = tokensTaken;
	tokenLock.unlock();
	return seen;
}

} // namespace

bool joinWaitsForTheThread() {
	__atomic_store_n(&joinValue, NOT_DONE, __ATOMIC_SEQ_CST);
	if (!joinThread.start(slowWorker, nullptr, stacks[FIRST_STACK], STACK_BYTES)) return false;

	bool startedTwice = joinThread.start(slowWorker, nullptr, stacks[FIRST_STACK], STACK_BYTES);
	bool timedOutEarly = !joinThread.join(EARLY_JOIN_MILLISECONDS);
	bool joined = joinThread.join(DEADLINE_MILLISECONDS);
	bool valueSet = __atomic_load_n(&joinValue, __ATOMIC_SEQ_CST) == JOIN_DONE_VALUE;
	bool joinedTwice = joinThread.join(NO_COUNT);
	if (startedTwice || !timedOutEarly || !joined || !valueSet || joinedTwice) return false;

	if (!joinThread.start(quickWorker, nullptr, stacks[FIRST_STACK], STACK_BYTES)) return false;
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	bool joinedLate = joinThread.join(DEADLINE_MILLISECONDS);

	JoinableThread neverStarted;
	JoinableThread tooSmall;
	bool refused = !tooSmall.start(quickWorker, nullptr, stacks[FIRST_STACK],
								   cock::abi::process::MIN_THREAD_STACK_BYTES - ADD_ONE);
	return joinedLate && !neverStarted.join(NO_COUNT) && refused && !tooSmall.join(NO_COUNT);
}

bool conditionVariablePassesTheTurn() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	turn = static_cast<uint32_t>(FIRST_PLAYER);
	turnsTaken = NO_COUNT;

	for (uintptr_t index = 0; index < PLAYER_COUNT; index++)
		if (!players[index].start(player, reinterpret_cast<void *>(index), stacks[index],
								  STACK_BYTES))
			return false;

	bool allJoined = true;
	for (size_t index = 0; index < PLAYER_COUNT; index++)
		allJoined = players[index].join(DEADLINE_MILLISECONDS) && allJoined;
	if (!allJoined || turnsTaken != PLAYER_COUNT * TURN_ROUNDS) return false;

	handoffLock.signalOnUnlock = &handoffChanged;
	if (!players[WAITER_THREAD].start(handoffWaiter, nullptr, stacks[WAITER_THREAD],
									  STACK_BYTES))
		return false;
	return players[WAITER_THREAD].join(DEADLINE_MILLISECONDS);
}

bool conditionVariableSignalsOneAndBroadcastsAll() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	tokens = NO_COUNT;
	tokensTaken = NO_COUNT;

	for (size_t index = 0; index < WAITER_COUNT; index++)
		if (!waiters[index].start(tokenWaiter, nullptr, stacks[index], STACK_BYTES))
			return false;
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	bool noneEarly = readTokensTaken() == NO_COUNT;

	tokenLock.lock();
	tokens = ONE_TOKEN;
	tokenLock.unlock();
	tokenChanged.signal();
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	bool exactlyOne = readTokensTaken() == ONE_TOKEN;

	tokenLock.lock();
	tokens = TWO_TOKENS;
	tokenLock.unlock();
	tokenChanged.broadcast();

	bool allJoined = true;
	for (size_t index = 0; index < WAITER_COUNT; index++)
		allJoined = waiters[index].join(DEADLINE_MILLISECONDS) && allJoined;
	return noneEarly && exactlyOne && allJoined && readTokensTaken() == WAITER_COUNT;
}

bool semaphoreCountsAndBlocks() {
	Semaphore counting(INITIAL_UNITS);
	bool counted = counting.tryWait() && counting.tryWait() && !counting.tryWait();
	counting.post();
	counted = counted && counting.tryWait() && !counting.tryWait();

	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&passed, NO_COUNT, __ATOMIC_SEQ_CST);
	for (size_t index = 0; index < WAITER_COUNT; index++)
		if (!waiters[index].start(gateWaiter, nullptr, stacks[index], STACK_BYTES))
			return false;

	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	bool blocked = __atomic_load_n(&passed, __ATOMIC_SEQ_CST) == NO_COUNT;
	gate.post();
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	bool exactlyOne = __atomic_load_n(&passed, __ATOMIC_SEQ_CST) == ONE_UNIT;
	gate.post();
	gate.post();

	bool allJoined = true;
	for (size_t index = 0; index < WAITER_COUNT; index++)
		allJoined = waiters[index].join(DEADLINE_MILLISECONDS) && allJoined;
	return counted && blocked && exactlyOne && allJoined &&
		   __atomic_load_n(&passed, __ATOMIC_SEQ_CST) == WAITER_COUNT;
}

} // namespace probe
