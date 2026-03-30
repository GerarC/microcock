#ifndef SHARED_COCK_RING_BUFFER_HPP
#define SHARED_COCK_RING_BUFFER_HPP

#include <stddef.h>

namespace cock::data_structure {

template <typename T, size_t Size> class RingBuffer {
  private:
	T buffer[Size];
	volatile size_t head;
	volatile size_t tail;

  public:
	constexpr RingBuffer() : head(0), tail(0) {}

	bool push(const T &item) {
		size_t next = (head + 1) % Size;
		if (next == tail) return false;
		buffer[head] = item;
		head = next;
		return true;
	}

	bool pop(T &item) {
		if (head == tail) return false;
		item = buffer[tail];
		tail = (tail + 1) % Size;
		return true;
	}

	bool isEmpty() const { return head == tail; }
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_RING_BUFFER_HPP
