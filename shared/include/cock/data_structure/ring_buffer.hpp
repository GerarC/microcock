#ifndef SHARED_COCK_RING_BUFFER_HPP
#define SHARED_COCK_RING_BUFFER_HPP

#include <stddef.h>

namespace cock::data_structure {

/**
 * @brief A fixed-size circular queue (Ring Buffer) implementation.
 * * Extremely useful for lock-free single-producer/single-consumer paradigms,
 * * such as passing IPC messages between processes safely.
 * @tparam T The data type to store in the buffer.
 * @tparam Size The maximum number of elements the buffer can hold.
 */
template <typename T, size_t Size> class RingBuffer {
  private:
	T buffer[Size];		  /**< Static array holding the buffer elements. */
	volatile size_t head; /**< Index where the next element will be written. */
	volatile size_t
		tail; /**< Index from where the next element will be read. */

  public:
	constexpr RingBuffer() : head(0), tail(0) {}

	/**
	 * @brief Pushes an item into the ring buffer.
	 * @param item The element to enqueue.
	 * @return true if the item was successfully pushed, false if the buffer is
	 * full.
	 */
	bool push(const T &item) {
		size_t next = (head + 1) % Size;
		if (next == tail) return false;
		buffer[head] = item;
		head = next;
		return true;
	}

	/**
	 * @brief Pops an item from the ring buffer.
	 * @param item Reference to store the dequeued element.
	 * @return true if an item was successfully popped, false if the buffer is
	 * empty.
	 */
	bool pop(T &item) {
		if (head == tail) return false;
		item = buffer[tail];
		tail = (tail + 1) % Size;
		return true;
	}

	/**
	 * @brief Checks if the ring buffer is empty.
	 * @return true if empty, false otherwise.
	 */
	bool isEmpty() const { return head == tail; }
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_RING_BUFFER_HPP
