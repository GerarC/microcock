#ifndef SHARED_COCK_LINKED_LIST_HPP
#define SHARED_COCK_LINKED_LIST_HPP

#include <cock/data_structure/iterator.hpp>
#include <cock/data_structure/node.hpp>
#include <stddef.h>

namespace cock::data_structure {

/**
 * @brief A generic doubly-linked list implementation.
 * * Designed for freestanding environments without STL availability.
 * @tparam TYPE The data type to be stored in the list nodes.
 */
template <typename TYPE> class LinkedList {
  private:
	Node<TYPE> *head;
	Node<TYPE> *tail;
	size_t lenght;

  public:
	LinkedList() : head(nullptr), tail(nullptr), lenght(0) {}

	TYPE &front() { return head->data; }

	TYPE &back() { return tail->data; }

	/**
	 * @brief Adds a new element to the end of the list.
	 * @param data The value to append.
	 */
	void append(const TYPE &data) {
		Node<TYPE> *new_node = new Node(data);
		if (!head) head = tail = new_node;
		else {
			tail->next = new_node;
			new_node->prev = tail;
			tail = new_node;
		}
		lenght++;
	}

	/**
	 * @brief Adds a new element to the beginning of the list.
	 * @param data The value to prepend.
	 */
	void preprend(const TYPE &data) {
		Node<TYPE> *new_node = new Node(data);
		if (!head) head = tail = new_node;
		else {
			new_node->next = head;
			head->prev = new_node;
			head = new_node;
		}
		lenght++;
	}

	/**
	 * @brief Removes the last element of the list.
	 * @param out Reference to store the popped value.
	 * @return true if an element was popped, false if the list was empty.
	 */
	bool pop(TYPE &out) {
		if (!tail) return false;

		Node<TYPE> *temp = tail;
		out = temp->data;
		tail = head->prev;

		if (tail) tail->next = nullptr;
		else head = nullptr;

		delete temp;
		lenght--;
		return true;
	}

	/**
	 * @brief Removes the first element of the list.
	 * @param out Reference to store the popped value.
	 * @return true if an element was popped, false if the list was empty.
	 */
	bool popFront(TYPE &out) {
		if (!head) return false;

		Node<TYPE> *temp = head;
		out = temp->data;
		head = head->next;

		if (head) head->prev = nullptr;
		else tail = nullptr;

		delete temp;
		lenght--;
		return true;
	}

	/**
	 * @brief Removes a specific node from the list by pointer.
	 */
	bool removeNode(Node<TYPE> *target) {
		if (!target) return false;

		if (target->prev) target->prev->next = target->next;
		else head = target->next;

		if (target->next) target->next->prev = target->prev;
		else tail = target->prev;

		delete target;
		lenght--;
	}

	/**
	 * @brief Searches for and removes the first occurrence of a specific value.
	 */
	bool remove(const TYPE &value) {
		for (Node<TYPE> *curr = head; curr != nullptr; curr = curr->next) {
			if (curr->data == value) {
				removeNode(curr);
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief Deletes all nodes and resets the list.
	 */
	void clear() {
		Node<TYPE> *current = head;
		while (current) {
			Node<TYPE> *next = current->next;
			delete current;
			current = next;
		}
		head = tail = nullptr;
		lenght = 0;
	}

	/**
	 * @brief Finds an element and returns an iterator pointing to it.
	 */
	Iterator<TYPE> find(const TYPE &value) {
		for (Node<TYPE> *curr = head; curr != nullptr; curr = curr->next) {
			if (curr->data == value) { return Iterator(curr); }
		}
		return end();
	}

	bool isEmpty() const { return lenght == 0; }
	size_t size() const { return lenght; }

	Iterator<TYPE> begin() { return Iterator<TYPE>(head); }
	Iterator<TYPE> end() { return Iterator<TYPE>(nullptr); }

	~LinkedList() { clear(); }
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_LINKED_LIST_HPP
