#ifndef CORE_HEAP_HPP
#define CORE_HEAP_HPP
#include <stddef.h>

namespace cock::core::memory {

typedef struct memory_block_t {
	size_t size;
	bool is_free;
	struct memory_block_t *next;
} MemoryBlock;

class Heap {
  private:
	static constexpr size_t PAGE_SIZE = 0x1000;
	static constexpr size_t ALIGNMENT = 0x0008;
	static inline size_t alignUp(size_t size) {
		return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
	}

	MemoryBlock *head;
    void coalesce();
    void split(MemoryBlock* block, size_t needed_size);
    void insertFreeBlock(MemoryBlock* block);
    void removeFreeBlock(MemoryBlock* block, MemoryBlock* prev);
    MemoryBlock* findFirstAvailable(size_t size, MemoryBlock** out_prev);

  public:
	constexpr Heap() : head(nullptr) {};

    void* allocate(size_t size);
    void free(void* ptr);
    void* callocate(size_t num, size_t size);
    void* reallocate(void* ptr, size_t size);
};

extern Heap kernel_heap;

void *kmalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t size);

} // namespace cock::core::memory

#endif // !CORE_HEAP_HPP
