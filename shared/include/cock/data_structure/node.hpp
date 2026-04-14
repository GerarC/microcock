#ifndef SHARED_COCK_NODE_HPP
#define SHARED_COCK_NODE_HPP

namespace cock::data_structure {

/**
 * @brief A templated node for a doubly-linked list.
 * @tparam TYPE The data type stored in the node.
 */
template <typename TYPE> class Node {
  public:
	TYPE data;		  /**< The stored payload data. */
	Node<TYPE> *next; /**< Pointer to the next node in the sequence. */
	Node<TYPE> *prev; /**< Pointer to the previous node in the sequence. */

	/**
	 * @brief Constructs a new Node with the given data.
	 * @param data The payload to store.
	 */
	Node(const TYPE &data) : data(data), next(nullptr), prev(nullptr) {}
};

} // namespace cock::data_structure

#endif // !SHARED_COCK_NODE_HPP
