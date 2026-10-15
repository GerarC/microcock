#ifndef CORE_HEAP_HPP
#define CORE_HEAP_HPP
#include <hal/page.hpp>
#include <stddef.h>

namespace cock::core::memory {

constexpr size_t HEAP_ALIGNMENT = alignof(max_align_t);

static_assert(HEAP_ALIGNMENT >= 2 && (HEAP_ALIGNMENT & (HEAP_ALIGNMENT - 1)) == 0,
			  "heap alignment must be a power of two");

struct alignas(HEAP_ALIGNMENT) MemoryBlock {
	static constexpr size_t FREE_FLAG = 1;

	size_t size_and_state;
	MemoryBlock *next;

	size_t size() const { return size_and_state & ~FREE_FLAG; }
	bool isFree() const { return (size_and_state & FREE_FLAG) != 0; }

	void set(size_t size, bool is_free) {
		size_and_state = size | (is_free ? FREE_FLAG : 0);
	}
	void setSize(size_t size) { set(size, isFree()); }
	void setFree(bool is_free) { set(size(), is_free); }
};

static_assert(sizeof(MemoryBlock) % HEAP_ALIGNMENT == 0,
			  "block header must keep payloads aligned");

class Heap {
  private:
	static constexpr size_t ALIGNMENT = HEAP_ALIGNMENT;
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
