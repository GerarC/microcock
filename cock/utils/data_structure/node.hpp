#ifndef COCK_NODE_HPP
#define COCK_NODE_HPP

namespace cock::utils::data_structure {

template <typename TYPE> class Node {
  public:
	TYPE data;
	Node<TYPE> *next;
	Node<TYPE> *prev;

	Node(const TYPE &data) : data(data), next(nullptr), prev(nullptr) {}
};

} // namespace cock::utils::data_structure

#endif // !COCK_NODE_HPP
