#ifndef CORE_HEAP_HPP
#define CORE_HEAP_HPP
#include <stddef.h>

namespace cock::core::memory {

/**
 * @brief Represents the metadata header for a dynamic memory block in the heap.
 */
typedef struct memory_block_t {
	size_t size; /**< Size of the usable memory block (excluding this header). */
	bool is_free; /**< Flag indicating if the block is available for allocation.
				   */
	struct memory_block_t
		*next; /**< Pointer to the next memory block in the free list. */
} MemoryBlock;

/**
 * @brief Custom Kernel Heap allocator using a First-Fit linked list algorithm.
 * * Supports block splitting to reduce internal fragmentation and block
 * coalescing
 * * to reduce external fragmentation.
 */
class Heap {
  private:
	static constexpr size_t PAGE_SIZE = 0x1000;
	static constexpr size_t ALIGNMENT = 0x0008;

	/**
	 * @brief Aligns a requested size to the defined byte alignment.
	 */
	static inline size_t alignUp(size_t size) {
		return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
	}

	MemoryBlock *head;

	/** @brief Merges adjacent free blocks to create larger contiguous memory
	 * regions. */
	void coalesce();

	/** @brief Splits a large free block into a required size block and a
	 * remainder free block. */
	void split(MemoryBlock *block, size_t needed_size);

	void insertFreeBlock(MemoryBlock *block);
	void removeFreeBlock(MemoryBlock *block, MemoryBlock *prev);
	MemoryBlock *findFirstAvailable(size_t size, MemoryBlock **out_prev);

  public:
	constexpr Heap() : head(nullptr) {};

	/**
	 * @brief Allocates a block of dynamic memory.
	 * @param size The requested size in bytes.
	 * @return Pointer to the allocated memory, or nullptr on failure.
	 */
	void *allocate(size_t size);

	/**
	 * @brief Frees a previously allocated memory block.
	 * @param ptr Pointer to the memory block to free.
	 */
	void free(void *ptr);

	/**
	 * @brief Allocates a block of memory for an array and initializes it to
	 * zero.
	 */
	void *callocate(size_t num, size_t size);

	/**
	 * @brief Resizes a previously allocated memory block.
	 */
	void *reallocate(void *ptr, size_t size);
};

extern Heap kernel_heap;

// Standard C-like heap API bindings
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t size);

} // namespace cock::core::memory

#endif // !CORE_HEAP_HPP
