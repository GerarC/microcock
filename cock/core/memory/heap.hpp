/** @file
 * @brief Kernel heap: a first fit free list allocator with splitting and
 * coalescing, and the kmalloc family built on it.
 */
#ifndef CORE_HEAP_HPP
#define CORE_HEAP_HPP
#include <hal/page.hpp>
#include <stddef.h>

namespace cock::core::memory {

/** @brief Alignment of every payload the heap returns. */
constexpr size_t HEAP_ALIGNMENT = alignof(max_align_t);

static_assert(HEAP_ALIGNMENT >= 2 && (HEAP_ALIGNMENT & (HEAP_ALIGNMENT - 1)) == 0,
			  "heap alignment must be a power of two");

/**
 * @brief Header placed before every heap payload.
 *
 * The lowest bit of the size field holds the free state, which is safe
 * because sizes are multiples of the alignment. While a block is free, next
 * links it into the address ordered free list.
 */
struct alignas(HEAP_ALIGNMENT) MemoryBlock {
	/** @brief Bit of size_and_state set when the block is free. */
	static constexpr size_t FREE_FLAG = 1;

	size_t size_and_state; /**< Payload size in bytes ORed with FREE_FLAG. */
	MemoryBlock *next; /**< Next free block by address, valid when free. */

	/** @return Payload size in bytes, without the header. */
	size_t size() const { return size_and_state & ~FREE_FLAG; }
	/** @return True when the block is on the free list. */
	bool isFree() const { return (size_and_state & FREE_FLAG) != 0; }

	/**
	 * @brief Sets size and state at once.
	 * @param size Payload size, a multiple of HEAP_ALIGNMENT.
	 * @param is_free New state.
	 */
	void set(size_t size, bool is_free) {
		size_and_state = size | (is_free ? FREE_FLAG : 0);
	}
	/**
	 * @brief Changes the size and keeps the state.
	 * @param size New payload size.
	 */
	void setSize(size_t size) { set(size, isFree()); }
	/**
	 * @brief Changes the state and keeps the size.
	 * @param is_free New state.
	 */
	void setFree(bool is_free) { set(size(), is_free); }
};

static_assert(sizeof(MemoryBlock) % HEAP_ALIGNMENT == 0,
			  "block header must keep payloads aligned");

/**
 * @brief Custom Kernel Heap allocator using a First-Fit linked list algorithm.
 * * Supports block splitting to reduce internal fragmentation and block
 * coalescing
 * * to reduce external fragmentation.
 *
 * Every public operation runs inside an InterruptGuard, so the heap is safe to
 * use from process context and from interrupt handlers on a single CPU. When no
 * free block fits, it grows by taking whole pages from the virtual memory
 * manager. Memory is never returned to it.
 */
class Heap {
  private:
	/** @brief Allocation granularity in bytes. */
	static constexpr size_t ALIGNMENT = HEAP_ALIGNMENT;
	/**
	 * @brief Rounds a size up to the allocation granularity.
	 * @param size Size in bytes.
	 * @return The smallest multiple of ALIGNMENT not below @p size.
	 */
	static inline size_t alignUp(size_t size) {
		return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
	}

	MemoryBlock *head; /**< First free block, the list is sorted by address. */

	/** @brief Merges adjacent free blocks to create larger contiguous memory
	 * regions. */
	void coalesce();

	/** @brief Splits a large free block into a required size block and a
	 * remainder free block. */
	void split(MemoryBlock *block, size_t needed_size);

	/**
	 * @brief Puts a block on the free list at its address ordered position and
	 * marks it free.
	 * @param block Block to insert.
	 */
	void insertFreeBlock(MemoryBlock *block);
	/**
	 * @brief Takes a block off the free list and marks it used.
	 * @param block Block to remove.
	 * @param prev Its predecessor on the list, or nullptr when it is the head.
	 */
	void removeFreeBlock(MemoryBlock *block, MemoryBlock *prev);
	/**
	 * @brief Finds the first free block with at least the given payload.
	 * @param size Payload size, already aligned.
	 * @param out_prev Receives the predecessor of the result, may be null.
	 * @return The block, or nullptr when none fits.
	 */
	MemoryBlock *findFirstAvailable(size_t size, MemoryBlock **out_prev);

  public:
	/** @brief Creates an empty heap that owns no memory yet. */
	constexpr Heap() : head(nullptr) {};

	/**
	 * @brief Allocates a block of dynamic memory.
	 * @param size The requested size in bytes.
	 * @return Pointer to the allocated memory, or nullptr on failure.
	 * Failure means a zero size, a size so large it would overflow, or no
	 * memory left to grow the heap. The memory is not cleared.
	 * @note Blocks interrupts while it runs.
	 */
	void *allocate(size_t size);

	/**
	 * @brief Frees a previously allocated memory block.
	 * @param ptr Pointer to the memory block to free.
	 * @pre @p ptr is null or came from this heap. A pointer already freed is
	 * ignored when its block is still marked free, anything else is undefined.
	 */
	void free(void *ptr);

	/**
	 * @brief Allocates a block of memory for an array and initializes it to
	 * zero.
	 * @param num Number of elements.
	 * @param size Size of one element in bytes.
	 * @return Pointer to zeroed memory, or nullptr when the product overflows
	 * or allocate fails.
	 */
	void *callocate(size_t num, size_t size);

	/**
	 * @brief Resizes a previously allocated memory block.
	 * @param ptr Block to resize. A null pointer behaves like allocate.
	 * @param size New size. Zero frees the block and returns nullptr.
	 * @return The same pointer when the block is already large enough,
	 * otherwise a new block holding a copy of the old contents. On failure
	 * returns nullptr and the old block stays valid.
	 */
	void *reallocate(void *ptr, size_t size);
};

/** @brief The one kernel heap instance behind kmalloc and friends. */
extern Heap kernel_heap;

/**
 * @brief Allocates kernel memory. See Heap::allocate.
 * @param size Bytes requested.
 * @return Aligned memory or nullptr on failure.
 */
void *kmalloc(size_t size);
/**
 * @brief Frees kernel memory. See Heap::free.
 * @param ptr Pointer from kmalloc, kcalloc or krealloc, or null.
 */
void kfree(void *ptr);
/**
 * @brief Allocates zeroed kernel memory for an array. See Heap::callocate.
 * @param num Number of elements.
 * @param size Size of one element.
 * @return Zeroed memory or nullptr on failure.
 */
void *kcalloc(size_t num, size_t size);
/**
 * @brief Resizes kernel memory. See Heap::reallocate.
 * @param ptr Block to resize, or null.
 * @param size New size in bytes.
 * @return The resized block or nullptr on failure.
 */
void *krealloc(void *ptr, size_t size);

} // namespace cock::core::memory

#endif // !CORE_HEAP_HPP
