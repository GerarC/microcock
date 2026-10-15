#include <cock/data_structure/ring_buffer.hpp>
#include <host_test/check.hpp>
#include <stddef.h>

namespace host_test {

using cock::data_structure::RingBuffer;

namespace {

constexpr size_t BUFFER_SIZE = 4;
constexpr size_t USABLE_SLOTS = BUFFER_SIZE - 1;
constexpr int FIRST_VALUE = 10;
constexpr int SECOND_VALUE = 20;
constexpr int THIRD_VALUE = 30;
constexpr int FOURTH_VALUE = 40;
constexpr int LAPS = 3;

void checkEmpty() {
	RingBuffer<int, BUFFER_SIZE> buffer;
	int value = 0;
	CHECK_TRUE(buffer.isEmpty());
	CHECK_FALSE(buffer.pop(value));
}

void checkOrder() {
	RingBuffer<int, BUFFER_SIZE> buffer;
	int value = 0;
	CHECK_TRUE(buffer.push(FIRST_VALUE));
	CHECK_TRUE(buffer.push(SECOND_VALUE));
	CHECK_FALSE(buffer.isEmpty());
	CHECK_TRUE(buffer.pop(value));
	CHECK_EQ(FIRST_VALUE, value);
	CHECK_TRUE(buffer.pop(value));
	CHECK_EQ(SECOND_VALUE, value);
	CHECK_TRUE(buffer.isEmpty());
}

void checkFull() {
	RingBuffer<int, BUFFER_SIZE> buffer;
	int value = 0;
	size_t pushed = 0;
	while (buffer.push(FIRST_VALUE) && pushed <= BUFFER_SIZE) pushed++;
	CHECK_EQ(USABLE_SLOTS, pushed);
	CHECK_FALSE(buffer.push(SECOND_VALUE));

	CHECK_TRUE(buffer.pop(value));
	CHECK_TRUE(buffer.push(THIRD_VALUE));
	CHECK_FALSE(buffer.push(FOURTH_VALUE));
}

void checkWrapAround() {
	RingBuffer<int, BUFFER_SIZE> buffer;
	int value = 0;
	for (int lap = 0; lap < LAPS * static_cast<int>(BUFFER_SIZE); lap++) {
		CHECK_TRUE(buffer.push(lap));
		CHECK_TRUE(buffer.pop(value));
		CHECK_EQ(lap, value);
	}
	CHECK_TRUE(buffer.isEmpty());
}

} // namespace

void testRingBuffer() {
	checkEmpty();
	checkOrder();
	checkFull();
	checkWrapAround();
}

} // namespace host_test
