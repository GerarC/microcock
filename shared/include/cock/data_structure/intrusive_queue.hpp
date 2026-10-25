/**
 * @file intrusive_queue.hpp
 * @brief FIFO queue whose links live inside the queued objects (no allocation).
 *
 * Not thread safe: the caller serializes access (the kernel uses it with interrupts disabled or under the scheduler lock).
 */
#ifndef SHARED_COCK_INTRUSIVE_QUEUE_HPP
#define SHARED_COCK_INTRUSIVE_QUEUE_HPP

namespace cock::data_structure {

/**
 * @brief Singly linked FIFO queue over objects that carry their own next pointer.
 * @tparam T Type of the queued objects.
 * @tparam Next Pointer to the member of `T` that holds the link to the next object. An object can be in at most one queue per link member.
 */
template <typename T, T *T::*Next> class IntrusiveQueue {
  private:
	/** Oldest element, or null when empty. */
	T *head;
	/** Newest element, or null when empty. */
	T *tail;

  public:
	/** Creates an empty queue. */
	constexpr IntrusiveQueue() : head(nullptr), tail(nullptr) {}

	/**
	 * @brief Tells whether the queue has no elements.
	 * @return True when empty.
	 */
	bool isEmpty() const { return head == nullptr; }

	/**
	 * @brief Appends an element at the tail.
	 * @param item Object to queue; must not be null and must not already be in a queue that uses the same link member. Its link is reset to null.
	 */
	void push(T *item) {
		item->*Next = nullptr;
		if (tail) tail->*Next = item;
		else head = item;
		tail = item;
	}

	/**
	 * @brief Removes the element at the head.
	 * @return The oldest element with its link cleared, or null when the queue is empty.
	 */
	T *pop() {
		T *item = head;
		if (!item) return nullptr;
		head = item->*Next;
		if (!head) tail = nullptr;
		item->*Next = nullptr;
		return item;
	}

	/**
	 * @brief Takes one element out of the queue wherever it is.
	 * @param item Object to remove; it may be absent.
	 * @return True when it was in the queue; its link is cleared. False leaves the queue as it was.
	 */
	bool remove(T *item) {
		T *previous = nullptr;
		for (T *current = head; current != nullptr; previous = current, current = current->*Next) {
			if (current != item) continue;
			T *next = current->*Next;
			if (previous) previous->*Next = next;
			else head = next;
			if (tail == current) tail = previous;
			current->*Next = nullptr;
			return true;
		}
		return false;
	}
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_INTRUSIVE_QUEUE_HPP
