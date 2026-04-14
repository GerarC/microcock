#ifndef SHARED_COCK_ITERATOR_HPP
#define SHARED_COCK_ITERATOR_HPP

#include <cock/data_structure/node.hpp>

namespace cock::data_structure {

/**
 * @brief A forward/backward iterator for the custom LinkedList.
 * * Allows for seamless traversal of list elements using standard range-based
 * loops.
 * @tparam TYPE The data type stored within the list's nodes.
 */
template <typename TYPE> class Iterator {
  private:
	Node<TYPE> *current; /**< Pointer to the current node in the iteration. */

  public:
	/**
	 * @brief Constructs an iterator starting at the given node.
	 * @param node The starting node.
	 */
	Iterator(Node<TYPE> *node) : current(node) {}

	/**
	 * @brief Dereferences the iterator to access the underlying data.
	 * @return A reference to the data payload.
	 */
	TYPE &operator*() { return current->data; }

	/** @brief Pre-increment operator (advances to the next node). */
	Iterator &operator++() {
		if (current) current = current->next;
		return *this;
	}

	/** @brief Post-increment operator. */
	Iterator operator++(int) {
		Iterator temp = *this;
		++(*this);
		return temp;
	}

	/** @brief Pre-decrement operator (moves to the previous node). */
	Iterator &operator--() {
		if (current) current = current->prev;
		return *this;
	}

	/** @brief Post-decrement operator. */
	Iterator &operator--(int) {
		Iterator temp = *this;
		--(*this);
		return temp;
	}

	/** @brief Equality comparison operator. */
	bool operator==(const Iterator<TYPE> other) const {
		return current == other.current;
	}

	/** @brief Inequality comparison operator. */
	bool operator!=(const Iterator<TYPE> other) const {
		return current != other.current;
	}
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_ITERATOR_HPP
