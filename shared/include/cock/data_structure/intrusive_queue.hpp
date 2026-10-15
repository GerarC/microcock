#ifndef SHARED_COCK_INTRUSIVE_QUEUE_HPP
#define SHARED_COCK_INTRUSIVE_QUEUE_HPP

namespace cock::data_structure {

template <typename T, T *T::*Next> class IntrusiveQueue {
  private:
	T *head;
	T *tail;

  public:
	constexpr IntrusiveQueue() : head(nullptr), tail(nullptr) {}

	bool isEmpty() const { return head == nullptr; }

	void push(T *item) {
		item->*Next = nullptr;
		if (tail) tail->*Next = item;
		else head = item;
		tail = item;
	}

	T *pop() {
		T *item = head;
		if (!item) return nullptr;
		head = item->*Next;
		if (!head) tail = nullptr;
		item->*Next = nullptr;
		return item;
	}
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_INTRUSIVE_QUEUE_HPP
