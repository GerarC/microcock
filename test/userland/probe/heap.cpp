#include "probe.hpp"
#include <cock/user/api.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace probe {

namespace {

constexpr size_t SMALL_ALLOCATION = 100;
constexpr size_t GROWN_ALLOCATION = 4000;
constexpr size_t ARRAY_LENGTH = 64;
constexpr size_t OVERFLOW_FACTOR = 2;
constexpr uint8_t SECOND_PATTERN = 0x3C;

bool holdsPattern(const void *memory, size_t length, uint8_t pattern) {
	const uint8_t *bytes = static_cast<const uint8_t *>(memory);
	for (size_t idx = 0; idx < length; idx++)
		if (bytes[idx] != pattern) return false;
	return true;
}

constexpr size_t HEAP_WORKERS = 3;
constexpr size_t HEAP_STACK_BYTES = 0x4000;
constexpr size_t HEAP_SLOTS = 128;
constexpr uint32_t HEAP_ROUNDS = 300000;
constexpr size_t HEAP_MIN_REQUEST = 1;
constexpr size_t HEAP_SIZE_STEP = 37;
constexpr size_t HEAP_SIZE_SPAN = 1500;
constexpr uint8_t HEAP_PATTERN_BASE = 0x40;
constexpr uint32_t HEAP_NO_COUNT = 0;
constexpr uint32_t HEAP_WAKE_EVERYONE = 0xFFFF;

struct HeapWorker {
	uint32_t id;
	size_t sizes[HEAP_SLOTS];
	uint8_t *blocks[HEAP_SLOTS];
};

alignas(PAGE_BYTES) uint8_t heapStacks[HEAP_WORKERS][HEAP_STACK_BYTES];
HeapWorker heapWorkers[HEAP_WORKERS];
uint32_t heapFinished = HEAP_NO_COUNT;
uint32_t heapFailures = HEAP_NO_COUNT;

void recordFailure() { __atomic_add_fetch(&heapFailures, 1, __ATOMIC_SEQ_CST); }

void retire(HeapWorker &worker, size_t slot, uint8_t pattern) {
	if (!worker.blocks[slot]) return;
	if (!holdsPattern(worker.blocks[slot], worker.sizes[slot], pattern)) recordFailure();
	free(worker.blocks[slot]);
	worker.blocks[slot] = nullptr;
}

void heapWorkerMain(void *argument) {
	HeapWorker &worker = *static_cast<HeapWorker *>(argument);
	uint8_t pattern = static_cast<uint8_t>(HEAP_PATTERN_BASE + worker.id);

	for (uint32_t round = 0; round < HEAP_ROUNDS; round++) {
		size_t slot = round % HEAP_SLOTS;
		retire(worker, slot, pattern);

		size_t size = HEAP_MIN_REQUEST + (round * HEAP_SIZE_STEP) % HEAP_SIZE_SPAN;
		uint8_t *block = static_cast<uint8_t *>(malloc(size));
		if (!block) {
			recordFailure();
			continue;
		}
		memset(block, pattern, size);
		worker.blocks[slot] = block;
		worker.sizes[slot] = size;
	}
	for (size_t slot = 0; slot < HEAP_SLOTS; slot++) retire(worker, slot, pattern);

	__atomic_add_fetch(&heapFinished, 1, __ATOMIC_SEQ_CST);
	cock::user::futex_wake(&heapFinished, HEAP_WAKE_EVERYONE);
}

void waitForHeapWorkers() {
	for (;;) {
		uint32_t seen = __atomic_load_n(&heapFinished, __ATOMIC_SEQ_CST);
		if (seen >= HEAP_WORKERS) return;
		cock::user::futex_wait(&heapFinished, seen);
	}
}

} // namespace

bool mallocIsSafeAcrossThreads() {
	__atomic_store_n(&heapFinished, HEAP_NO_COUNT, __ATOMIC_SEQ_CST);
	__atomic_store_n(&heapFailures, HEAP_NO_COUNT, __ATOMIC_SEQ_CST);

	for (size_t idx = 0; idx < HEAP_WORKERS; idx++) {
		heapWorkers[idx].id = static_cast<uint32_t>(idx);
		if (cock::user::thread_start(heapWorkerMain, &heapWorkers[idx], heapStacks[idx],
									 HEAP_STACK_BYTES) == cock::user::NO_PROCESS)
			return false;
	}

	waitForHeapWorkers();
	if (__atomic_load_n(&heapFailures, __ATOMIC_SEQ_CST) != HEAP_NO_COUNT) return false;

	void *after = malloc(SMALL_ALLOCATION);
	free(after);
	return after != nullptr;
}

bool mallocReusesAndCoalesces() {
	uint8_t *first = static_cast<uint8_t *>(malloc(SMALL_ALLOCATION));
	uint8_t *second = static_cast<uint8_t *>(malloc(SMALL_ALLOCATION));
	if (!first || !second || first == second) return false;

	memset(first, FIRST_PATTERN, SMALL_ALLOCATION);
	memset(second, SECOND_PATTERN, SMALL_ALLOCATION);
	if (!holdsPattern(first, SMALL_ALLOCATION, FIRST_PATTERN)) return false;

	free(first);
	void *reused = malloc(SMALL_ALLOCATION);
	if (reused != first) return false;

	free(reused);
	free(second);
	void *merged = malloc(SMALL_ALLOCATION * OVERFLOW_FACTOR);
	bool coalesced = merged == first;
	free(merged);
	return coalesced;
}

bool reallocKeepsContents() {
	void *start = realloc(nullptr, SMALL_ALLOCATION);
	if (!start) return false;
	memset(start, FIRST_PATTERN, SMALL_ALLOCATION);

	void *grown = realloc(start, GROWN_ALLOCATION);
	if (!grown) return false;
	bool kept = holdsPattern(grown, SMALL_ALLOCATION, FIRST_PATTERN);
	free(grown);
	return kept;
}

bool callocZeroesRecycledMemory() {
	uint8_t *dirty = static_cast<uint8_t *>(malloc(SMALL_ALLOCATION));
	if (!dirty) return false;
	memset(dirty, FIRST_PATTERN, SMALL_ALLOCATION);
	free(dirty);

	void *clean = calloc(ARRAY_LENGTH, sizeof(uint8_t));
	if (!clean) return false;
	bool zeroed = holdsPattern(clean, ARRAY_LENGTH, ZERO_PATTERN);
	free(clean);
	return zeroed && calloc(SIZE_MAX, OVERFLOW_FACTOR) == nullptr;
}

bool hugeAllocationFailsCleanly() {
	if (malloc(OVERSIZED_MAPPING) != nullptr) return false;
	if (malloc(SIZE_MAX) != nullptr) return false;

	void *small = malloc(SMALL_ALLOCATION);
	free(small);
	return small != nullptr;
}

bool operatorNewRoundTrips() {
	uint32_t *values = new uint32_t[ARRAY_LENGTH];
	for (size_t idx = 0; idx < ARRAY_LENGTH; idx++)
		values[idx] = static_cast<uint32_t>(idx);
	bool intact = true;
	for (size_t idx = 0; idx < ARRAY_LENGTH; idx++)
		intact = intact && values[idx] == idx;
	delete[] values;

	uint32_t *single = new uint32_t(PING_VALUE);
	intact = intact && *single == PING_VALUE;
	delete single;
	return intact;
}

} // namespace probe
