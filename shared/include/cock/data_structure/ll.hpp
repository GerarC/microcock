#ifndef SHARED_COCK_LINKED_LIST_HPP
#define SHARED_COCK_LINKED_LIST_HPP

#include <cock/data_structure/iterator.hpp>
#include <cock/data_structure/node.hpp>
#include <stddef.h>

namespace cock::data_structure {

template <typename TYPE> class LinkedList {
  private:
	Node<TYPE> *head;
	Node<TYPE> *tail;
	size_t lenght;

  public:
	LinkedList() : head(nullptr), tail(nullptr), lenght(0) {}

	TYPE &front() { return head->data; }

	TYPE &back() { return tail->data; }

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

	bool removeNode(Node<TYPE> *target) {
		if (!target) return false;

		if (target->prev) target->prev->next = target->next;
		else head = target->next;

		if (target->next) target->next->prev = target->prev;
		else tail = target->prev;

		delete target;
		lenght--;
	}

	bool remove(const TYPE &value) {
		for (Node<TYPE> *curr = head; curr != nullptr; curr = curr->next) {
			if (curr->data == value) {
				removeNode(curr);
				return true;
			}
		}
		return false;
	}

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

} // namespace cock::utils::data_structure

#endif // !SHARED_COCK_LINKED_LIST_HPP
