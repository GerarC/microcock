#include <cock/data_structure/ll.hpp>
#include <host_test/check.hpp>
#include <stddef.h>

namespace host_test {

using cock::data_structure::LinkedList;

namespace {

constexpr int FIRST_VALUE = 1;
constexpr int SECOND_VALUE = 2;
constexpr int THIRD_VALUE = 3;
constexpr int MISSING_VALUE = 99;
constexpr size_t NO_ELEMENTS = 0;
constexpr size_t TWO_ELEMENTS = 2;
constexpr size_t THREE_ELEMENTS = 3;

void checkEmpty() {
	LinkedList<int> list;
	int value = 0;
	CHECK_TRUE(list.isEmpty());
	CHECK_EQ(NO_ELEMENTS, list.size());
	CHECK_FALSE(list.pop(value));
	CHECK_FALSE(list.popFront(value));
	CHECK_FALSE(list.remove(MISSING_VALUE));
	CHECK_FALSE(list.removeNode(nullptr));
	CHECK_TRUE(list.begin() == list.end());
}

void checkAppendAndPrepend() {
	LinkedList<int> list;
	list.append(SECOND_VALUE);
	list.append(THIRD_VALUE);
	list.preprend(FIRST_VALUE);

	CHECK_EQ(THREE_ELEMENTS, list.size());
	CHECK_EQ(FIRST_VALUE, list.front());
	CHECK_EQ(THIRD_VALUE, list.back());

	int expected = FIRST_VALUE;
	for (auto it = list.begin(); it != list.end(); ++it) {
		CHECK_EQ(expected, *it);
		expected++;
	}
}

void checkPopEnds() {
	LinkedList<int> list;
	list.append(FIRST_VALUE);
	list.append(SECOND_VALUE);
	list.append(THIRD_VALUE);

	int value = 0;
	CHECK_TRUE(list.pop(value));
	CHECK_EQ(THIRD_VALUE, value);
	CHECK_TRUE(list.popFront(value));
	CHECK_EQ(FIRST_VALUE, value);
	CHECK_EQ(SECOND_VALUE, list.front());
	CHECK_EQ(SECOND_VALUE, list.back());
	CHECK_TRUE(list.pop(value));
	CHECK_TRUE(list.isEmpty());
}

void checkRemove() {
	LinkedList<int> list;
	list.append(FIRST_VALUE);
	list.append(SECOND_VALUE);
	list.append(THIRD_VALUE);

	CHECK_TRUE(list.remove(SECOND_VALUE));
	CHECK_EQ(TWO_ELEMENTS, list.size());
	CHECK_FALSE(list.remove(SECOND_VALUE));
	CHECK_TRUE(list.remove(FIRST_VALUE));
	CHECK_EQ(THIRD_VALUE, list.front());
	CHECK_TRUE(list.remove(THIRD_VALUE));
	CHECK_TRUE(list.isEmpty());
}

void checkFindAndClear() {
	LinkedList<int> list;
	list.append(FIRST_VALUE);
	list.append(SECOND_VALUE);

	CHECK_EQ(SECOND_VALUE, *list.find(SECOND_VALUE));
	CHECK_TRUE(list.find(MISSING_VALUE) == list.end());

	list.clear();
	CHECK_TRUE(list.isEmpty());
	CHECK_EQ(NO_ELEMENTS, list.size());
	list.append(THIRD_VALUE);
	CHECK_EQ(THIRD_VALUE, list.front());
}

} // namespace

void testLinkedList() {
	checkEmpty();
	checkAppendAndPrepend();
	checkPopEnds();
	checkRemove();
	checkFindAndClear();
}

} // namespace host_test
