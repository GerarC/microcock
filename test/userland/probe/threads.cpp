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

alignas(PAGE_BYTES) uint8_t stacks[STACK_COUNT][STACK_BYTES];

cock::user::Mutex lock;
uint32_t counter = 0;
uint32_t finished = 0;
uint32_t gate = GATE_CLOSED;

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
