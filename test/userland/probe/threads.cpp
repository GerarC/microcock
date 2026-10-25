#include "probe.hpp"
#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <cock/user/mutex.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>

using cock::user::NO_PROCESS;
using namespace cock::abi::process;

namespace probe {

namespace {

constexpr size_t STACK_BYTES = 0x4000;
constexpr size_t STACK_COUNT = MAX_THREADS_PER_PROCESS;
constexpr size_t WORKER_COUNT = 2;
constexpr uint32_t ROUNDS = 3;
constexpr uint32_t HOLD_MILLISECONDS = 20;
constexpr uint32_t SETTLE_MILLISECONDS = 50;
constexpr uint32_t SHORT_MILLISECONDS = 30;
constexpr uint32_t OVERSIZED_MILLISECONDS = 0x80000000;
constexpr uint32_t GATE_CLOSED = 0;
constexpr uint32_t GATE_OPEN = 1;
constexpr uint32_t WAKE_EVERYONE = 0xFFFF;
constexpr uint32_t WORD_VALUE = 5;
constexpr uint32_t OTHER_VALUE = 6;
constexpr uint32_t NO_WAITERS = 0;
constexpr size_t FIRST_STACK = 0;
constexpr size_t MISALIGNMENT = 1;
constexpr uint32_t NO_COUNT = 0;
constexpr size_t NO_THREADS = 0;
constexpr uint32_t PING_PONG_ROUNDS = 50;
constexpr uint32_t STALE_PAUSE_MILLISECONDS = 1;
constexpr uint32_t PING_PONG_WAITERS = 3;
constexpr uint32_t FIRST_ROUND = 1;
constexpr uint32_t LOST_WAKEUP_MILLISECONDS = 1000;
constexpr uint32_t NO_BITS = 0;
constexpr uint32_t ONE_THREAD = 1;

alignas(PAGE_BYTES) uint8_t stacks[STACK_COUNT][STACK_BYTES];

cock::user::Mutex lock;
uint32_t counter = 0;
uint32_t finished = 0;
uint32_t gate = GATE_CLOSED;
uint32_t roundWord = 0;
uint32_t acknowledged = 0;
bool wakeupLost = false;

void finish() {
	__atomic_add_fetch(&finished, 1, __ATOMIC_SEQ_CST);
	cock::user::futex_wake(&finished, WAKE_EVERYONE);
}

void waitFinished(uint32_t target) {
	for (;;) {
		uint32_t seen = __atomic_load_n(&finished, __ATOMIC_SEQ_CST);
		if (seen >= target) return;
		cock::user::futex_wait(&finished, seen);
	}
}

void incrementer(void *) {
	for (uint32_t round = 0; round < ROUNDS; round++) {
		lock.lock();
		cock::user::sleep_ms(HOLD_MILLISECONDS);
		counter = counter + 1;
		lock.unlock();
	}
	finish();
}

void gateWaiter(void *) {
	cock::user::futex_wait(&gate, GATE_CLOSED);
	finish();
}

void idle(void *) {}

void roundWaiter(void *) {
	for (uint32_t round = FIRST_ROUND; round <= PING_PONG_ROUNDS; round++) {
		for (;;) {
			uint32_t seen = __atomic_load_n(&roundWord, __ATOMIC_SEQ_CST);
			if (seen >= round) break;
			cock::user::sleep_ms(STALE_PAUSE_MILLISECONDS);
			if (cock::user::futex_wait(&roundWord, seen, LOST_WAKEUP_MILLISECONDS) ==
				FUTEX_TIMED_OUT) {
				__atomic_store_n(&wakeupLost, true, __ATOMIC_SEQ_CST);
				finish();
				return;
			}
		}
		__atomic_add_fetch(&acknowledged, 1, __ATOMIC_SEQ_CST);
		cock::user::futex_wake(&acknowledged, WAKE_EVERYONE);
	}
	finish();
}

} // namespace

bool threadsShareMemoryUnderMutex() {
	counter = 0;
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);

	for (size_t worker = 0; worker < WORKER_COUNT; worker++)
		if (cock::user::thread_start(incrementer, nullptr, stacks[FIRST_STACK + worker],
									 STACK_BYTES) == NO_PROCESS)
			return false;

	waitFinished(WORKER_COUNT);
	return counter == WORKER_COUNT * ROUNDS;
}

bool futexWakesEveryWaiterAndThreadsAreBounded() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	__atomic_store_n(&gate, GATE_CLOSED, __ATOMIC_SEQ_CST);

	size_t created = NO_THREADS;
	bool refused = false;
	for (size_t attempt = 0; attempt <= STACK_COUNT && !refused; attempt++) {
		size_t slot = attempt < STACK_COUNT ? attempt : FIRST_STACK;
		if (cock::user::thread_start(gateWaiter, nullptr, stacks[slot], STACK_BYTES) ==
			NO_PROCESS)
			refused = true;
		else
			created++;
	}
	if (!refused || created == NO_THREADS || created >= MAX_THREADS_PER_PROCESS)
		return false;

	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&gate, GATE_OPEN, __ATOMIC_SEQ_CST);
	uintptr_t woken = cock::user::futex_wake(&gate, WAKE_EVERYONE);
	waitFinished(static_cast<uint32_t>(created));
	return woken == created;
}

bool futexLosesNoWakeup() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	__atomic_store_n(&roundWord, NO_COUNT, __ATOMIC_SEQ_CST);
	__atomic_store_n(&acknowledged, NO_COUNT, __ATOMIC_SEQ_CST);
	__atomic_store_n(&wakeupLost, false, __ATOMIC_SEQ_CST);

	for (size_t waiter = 0; waiter < PING_PONG_WAITERS; waiter++)
		if (cock::user::thread_start(roundWaiter, nullptr, stacks[FIRST_STACK + waiter],
									 STACK_BYTES) == NO_PROCESS)
			return false;

	bool stalled = false;
	for (uint32_t round = FIRST_ROUND; round <= PING_PONG_ROUNDS && !stalled; round++) {
		__atomic_store_n(&roundWord, round, __ATOMIC_SEQ_CST);
		cock::user::futex_wake(&roundWord, WAKE_EVERYONE);

		uint32_t target = round * PING_PONG_WAITERS;
		for (;;) {
			uint32_t seen = __atomic_load_n(&acknowledged, __ATOMIC_SEQ_CST);
			if (seen >= target) break;
			if (cock::user::futex_wait(&acknowledged, seen, LOST_WAKEUP_MILLISECONDS) ==
				FUTEX_TIMED_OUT) {
				stalled = true;
				break;
			}
		}
	}

	waitFinished(PING_PONG_WAITERS);
	return !stalled && !__atomic_load_n(&wakeupLost, __ATOMIC_SEQ_CST) &&
		   __atomic_load_n(&acknowledged, __ATOMIC_SEQ_CST) ==
			   PING_PONG_ROUNDS * PING_PONG_WAITERS;
}

bool processStateReportsThreads() {
	cock::user::sleep_ms(SETTLE_MILLISECONDS);
	__atomic_store_n(&finished, NO_COUNT, __ATOMIC_SEQ_CST);
	__atomic_store_n(&gate, GATE_CLOSED, __ATOMIC_SEQ_CST);

	uint32_t self = cock::user::self();
	uint32_t alone = cock::user::process_state_mask(self);
	bool aloneRuns = (alone & THREAD_RUNNING) != NO_BITS && (alone & THREAD_BLOCKED) == NO_BITS;

	uint32_t waiter = cock::user::thread_start(gateWaiter, nullptr, stacks[FIRST_STACK], STACK_BYTES);
	if (waiter == NO_PROCESS) return false;
	cock::user::sleep_ms(SETTLE_MILLISECONDS);

	uint32_t together = cock::user::process_state_mask(self);
	bool sleeperSeen = (together & THREAD_RUNNING) != NO_BITS && (together & THREAD_BLOCKED) != NO_BITS;
	bool threadIdRefused = cock::user::process_state_mask(waiter) == cock::user::NO_STATE_MASK;

	__atomic_store_n(&gate, GATE_OPEN, __ATOMIC_SEQ_CST);
	cock::user::futex_wake(&gate, WAKE_EVERYONE);
	waitFinished(ONE_THREAD);
	cock::user::sleep_ms(SETTLE_MILLISECONDS);

	uint32_t finishedMask = cock::user::process_state_mask(self);
	bool sleeperGone = (finishedMask & THREAD_RUNNING) != NO_BITS && (finishedMask & THREAD_BLOCKED) == NO_BITS;
	bool unknownRefused = cock::user::process_state_mask(NO_PROCESS) == cock::user::NO_STATE_MASK &&
						  cock::user::process_state_mask(waiter) == cock::user::NO_STATE_MASK;

	return aloneRuns && sleeperSeen && threadIdRefused && sleeperGone && unknownRefused;
}

bool futexAndThreadCallsRefuseBadInput() {
	uint32_t word = WORD_VALUE;
	uintptr_t address = reinterpret_cast<uintptr_t>(&word);
	const uint32_t *misaligned = reinterpret_cast<const uint32_t *>(address + MISALIGNMENT);
	const uint32_t *kernel = reinterpret_cast<const uint32_t *>(KERNEL_ADDRESS);
	const uint32_t *unmapped = reinterpret_cast<const uint32_t *>(UNMAPPED_USER_ADDRESS);

	bool futexRefusals =
		cock::user::futex_wait(nullptr, WORD_VALUE) == cock::user::FAILURE &&
		cock::user::futex_wait(misaligned, WORD_VALUE) == cock::user::FAILURE &&
		cock::user::futex_wait(kernel, WORD_VALUE) == cock::user::FAILURE &&
		cock::user::futex_wait(unmapped, WORD_VALUE) == cock::user::FAILURE &&
		cock::user::futex_wake(nullptr, 1) == cock::user::FAILURE &&
		cock::user::futex_wake(misaligned, 1) == cock::user::FAILURE &&
		cock::user::futex_wake(kernel, 1) == cock::user::FAILURE &&
		cock::user::futex_wait(&word, WORD_VALUE, OVERSIZED_MILLISECONDS) ==
			cock::user::FAILURE;

	bool futexResults =
		cock::user::futex_wait(&word, OTHER_VALUE) == FUTEX_VALUE_CHANGED &&
		cock::user::futex_wait(&word, WORD_VALUE, NO_COUNT) == FUTEX_TIMED_OUT &&
		cock::user::futex_wait(&word, WORD_VALUE, SHORT_MILLISECONDS) ==
			FUTEX_TIMED_OUT &&
		cock::user::futex_wake(&word, 1) == NO_WAITERS;

	uintptr_t stack = reinterpret_cast<uintptr_t>(stacks[FIRST_STACK]);
	bool threadRefusals =
		cock::user::thread_create_raw(KERNEL_ADDRESS, stacks[FIRST_STACK],
									  STACK_BYTES) == NO_PROCESS &&
		cock::user::thread_create_raw(UNMAPPED_USER_ADDRESS, stacks[FIRST_STACK],
									  STACK_BYTES) == NO_PROCESS &&
		cock::user::thread_create_raw(reinterpret_cast<uintptr_t>(&idle),
									  reinterpret_cast<void *>(UNMAPPED_USER_ADDRESS),
									  STACK_BYTES) == NO_PROCESS &&
		cock::user::thread_create_raw(reinterpret_cast<uintptr_t>(&idle),
									  reinterpret_cast<void *>(KERNEL_ADDRESS),
									  STACK_BYTES) == NO_PROCESS &&
		cock::user::thread_create_raw(reinterpret_cast<uintptr_t>(&idle),
									  reinterpret_cast<void *>(stack),
									  MIN_THREAD_STACK_BYTES - 1) == NO_PROCESS;

	return futexRefusals && futexResults && threadRefusals;
}

} // namespace probe
