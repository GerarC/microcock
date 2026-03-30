#ifndef SHARED_COCK_NODE_HPP
#define SHARED_COCK_NODE_HPP

namespace cock::data_structure {

template <typename TYPE> class Node {
  public:
	TYPE data;
	Node<TYPE> *next;
	Node<TYPE> *prev;

	Node(const TYPE &data) : data(data), next(nullptr), prev(nullptr) {}
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_NODE_HPP
