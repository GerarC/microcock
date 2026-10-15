#include <cock/data_structure/range_pool.hpp>
#include <host_test/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using cock::data_structure::RangePool;

namespace {

constexpr uintptr_t BASE = 0x100000;
constexpr uintptr_t PAGE = 0x1000;
constexpr size_t TOTAL_PAGES = 64;
constexpr uintptr_t LIMIT = BASE + TOTAL_PAGES * PAGE;
constexpr size_t SLOTS = 4;
constexpr size_t ONE = 1;
constexpr size_t TWO = 2;
constexpr size_t THREE = 3;
constexpr size_t FOUR = 4;
constexpr size_t LAPS = 1000;

using Pool = RangePool<SLOTS>;

uintptr_t at(size_t page) { return BASE + page * PAGE; }

void checkBump() {
	Pool pool(BASE, LIMIT, PAGE);
	uintptr_t first = 0;
	uintptr_t second = 0;
	CHECK_TRUE(pool.reserve(TWO, first));
	CHECK_TRUE(pool.reserve(THREE, second));
	CHECK_EQ(BASE, first);
	CHECK_EQ(at(TWO), second);
	CHECK_EQ(at(TWO + THREE), pool.end());
	CHECK_FALSE(pool.reserve(0, first));
	CHECK_FALSE(pool.reserve(TOTAL_PAGES, first));
}

void checkReuse() {
	Pool pool(BASE, LIMIT, PAGE);
	uintptr_t first = 0;
	uintptr_t second = 0;
	uintptr_t third = 0;
	CHECK_TRUE(pool.reserve(TWO, first));
	CHECK_TRUE(pool.reserve(TWO, second));
	CHECK_TRUE(pool.reserve(TWO, third));
	CHECK_TRUE(pool.release(second, TWO));
	CHECK_EQ(ONE, pool.freeRanges());
	uintptr_t again = 0;
	CHECK_TRUE(pool.reserve(ONE, again));
	CHECK_EQ(second, again);
	CHECK_EQ(ONE, pool.freePages());
	CHECK_TRUE(pool.reserve(ONE, again));
	CHECK_EQ(second + PAGE, again);
	CHECK_EQ(size_t(0), pool.freeRanges());
}

void checkRetract() {
	Pool pool(BASE, LIMIT, PAGE);
	uintptr_t first = 0;
	uintptr_t second = 0;
	CHECK_TRUE(pool.reserve(TWO, first));
	CHECK_TRUE(pool.reserve(TWO, second));
	CHECK_TRUE(pool.release(first, TWO));
	CHECK_EQ(ONE, pool.freeRanges());
	CHECK_TRUE(pool.release(second, TWO));
	CHECK_EQ(BASE, pool.end());
	CHECK_EQ(size_t(0), pool.freeRanges());
}

void checkCoalesce() {
	Pool pool(BASE, LIMIT, PAGE);
	uintptr_t blocks[FOUR] = {};
	for (size_t index = 0; index < FOUR; index++)
		CHECK_TRUE(pool.reserve(ONE, blocks[index]));
	uintptr_t guard = 0;
	CHECK_TRUE(pool.reserve(ONE, guard));

	CHECK_TRUE(pool.release(blocks[0], ONE));
	CHECK_TRUE(pool.release(blocks[2], ONE));
	CHECK_EQ(TWO, pool.freeRanges());
	CHECK_TRUE(pool.release(blocks[1], ONE));
	CHECK_EQ(ONE, pool.freeRanges());
	CHECK_EQ(THREE, pool.freePages());

	uintptr_t run = 0;
	CHECK_TRUE(pool.reserve(THREE, run));
	CHECK_EQ(blocks[0], run);
}

void checkRefusals() {
	Pool pool(BASE, LIMIT, PAGE);
	uintptr_t first = 0;
	uintptr_t second = 0;
	CHECK_TRUE(pool.reserve(THREE, first));
	CHECK_TRUE(pool.reserve(ONE, second));

	CHECK_FALSE(pool.release(first, 0));
	CHECK_FALSE(pool.release(first + ONE, ONE));
	CHECK_FALSE(pool.release(BASE - PAGE, ONE));
	CHECK_FALSE(pool.release(first, FOUR + ONE));
	CHECK_FALSE(pool.release(LIMIT, ONE));

	CHECK_TRUE(pool.release(first, TWO));
	CHECK_FALSE(pool.release(first, ONE));
	CHECK_FALSE(pool.release(first + PAGE, TWO));
	CHECK_EQ(TWO, pool.freePages());
}

void checkFullTable() {
	Pool pool(BASE, LIMIT, PAGE);
	uintptr_t blocks[SLOTS * TWO + THREE] = {};
	size_t reserved = sizeof(blocks) / sizeof(blocks[0]);
	for (size_t index = 0; index < reserved; index++)
		CHECK_TRUE(pool.reserve(ONE, blocks[index]));

	for (size_t index = 0; index < SLOTS; index++)
		CHECK_TRUE(pool.release(blocks[index * TWO], ONE));
	CHECK_EQ(SLOTS, pool.freeRanges());
	CHECK_FALSE(pool.release(blocks[SLOTS * TWO], ONE));
	CHECK_EQ(SLOTS, pool.freeRanges());
	CHECK_TRUE(pool.release(blocks[reserved - ONE], ONE));
	CHECK_EQ(SLOTS, pool.freeRanges());
}

void checkLoopStaysFlat() {
	Pool pool(BASE, LIMIT, PAGE);
	uintptr_t keep = 0;
	CHECK_TRUE(pool.reserve(ONE, keep));
	uintptr_t high = pool.end();
	for (size_t lap = 0; lap < LAPS; lap++) {
		uintptr_t stack = 0;
		uintptr_t heap = 0;
		CHECK_TRUE(pool.reserve(FOUR + ONE, stack));
		CHECK_TRUE(pool.reserve(TWO, heap));
		CHECK_TRUE(pool.release(stack, FOUR + ONE));
		CHECK_TRUE(pool.release(heap, TWO));
		if (lap == 0) high = pool.end();
		CHECK_EQ(high, pool.end());
	}
	CHECK_EQ(size_t(0), pool.freeRanges());
}

} // namespace

void testRangePool() {
	checkBump();
	checkReuse();
	checkRetract();
	checkCoalesce();
	checkRefusals();
	checkFullTable();
	checkLoopStaysFlat();
}

} // namespace host_test
