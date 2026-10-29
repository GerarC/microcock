#include <cock/data_structure/intrusive_queue.hpp>
#include <test/host/check.hpp>

namespace host_test {

namespace {

struct Item {
	Item *link = nullptr;
	int id = 0;
};

using Queue = cock::data_structure::IntrusiveQueue<Item, &Item::link>;

constexpr int FIRST_ID = 1;
constexpr int SECOND_ID = 2;
constexpr int THIRD_ID = 3;

void checkEmpty() {
	Queue queue;
	CHECK_TRUE(queue.isEmpty());
	CHECK_TRUE(queue.pop() == nullptr);
}

void checkFifoOrder() {
	Queue queue;
	Item first;
	Item second;
	Item third;
	first.id = FIRST_ID;
	second.id = SECOND_ID;
	third.id = THIRD_ID;

	queue.push(&first);
	queue.push(&second);
	queue.push(&third);
	CHECK_FALSE(queue.isEmpty());

	CHECK_TRUE(queue.pop() == &first);
	CHECK_TRUE(queue.pop() == &second);
	CHECK_TRUE(queue.pop() == &third);
	CHECK_TRUE(queue.pop() == nullptr);
	CHECK_TRUE(queue.isEmpty());
}

void checkPopClearsLink() {
	Queue queue;
	Item first;
	Item second;
	queue.push(&first);
	queue.push(&second);

	Item *popped = queue.pop();
	CHECK_TRUE(popped == &first);
	CHECK_TRUE(popped->link == nullptr);
}

void checkReuse() {
	Queue queue;
	Item first;
	Item second;
	queue.push(&first);
	CHECK_TRUE(queue.pop() == &first);
	CHECK_TRUE(queue.isEmpty());

	queue.push(&second);
	queue.push(&first);
	CHECK_TRUE(queue.pop() == &second);
	CHECK_TRUE(queue.pop() == &first);
	CHECK_TRUE(queue.isEmpty());
}

void checkRemove() {
	Queue queue;
	Item first;
	Item second;
	Item third;
	Item absent;
	queue.push(&first);
	queue.push(&second);
	queue.push(&third);

	CHECK_FALSE(queue.remove(&absent));
	CHECK_TRUE(queue.remove(&second));
	CHECK_TRUE(second.link == nullptr);
	CHECK_FALSE(queue.remove(&second));
	CHECK_TRUE(queue.remove(&third));

	Item fourth;
	queue.push(&fourth);
	CHECK_TRUE(queue.pop() == &first);
	CHECK_TRUE(queue.pop() == &fourth);
	CHECK_TRUE(queue.isEmpty());

	queue.push(&first);
	CHECK_TRUE(queue.remove(&first));
	CHECK_TRUE(queue.isEmpty());
	queue.push(&second);
	CHECK_TRUE(queue.pop() == &second);
	CHECK_TRUE(queue.pop() == nullptr);
}

} // namespace

void testIntrusiveQueue() {
	checkEmpty();
	checkFifoOrder();
	checkPopClearsLink();
	checkReuse();
	checkRemove();
}

} // namespace host_test
