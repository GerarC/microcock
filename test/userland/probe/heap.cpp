#include "probe.hpp"
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

} // namespace

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
