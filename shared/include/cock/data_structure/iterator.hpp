#ifndef COCK_ITERATOR_HPP
#define COCK_ITERATOR_HPP

#include <cock/data_structure/node.hpp>

namespace cock::data_structure {

template <typename TYPE> class Iterator {
  private:
	Node<TYPE> *current;

  public:
	Iterator(Node<TYPE> *node) : current(node) {}
	TYPE &operator*() { return current->data; }

	Iterator &operator++() {
		if (current) current->next;
		return *this;
	}

	Iterator operator++(int) {
		Iterator temp = *this;
		++(*this);
		return temp;
	}

	Iterator &operator--() {
		if (current) current->prev;
		return *this;
	}

	Iterator &operator--(int) {
		Iterator temp = *this;
		--(*this);
		return temp;
	}

	bool operator==(const Iterator<TYPE> other) const {
		return current == other.current;
	}

	bool operator!=(const Iterator<TYPE> other) const {
		return current != other.current;
	}
};

} // namespace cock::utils::data_structure

#endif // !COCK_ITERATOR_HPP
