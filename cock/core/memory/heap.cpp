#include <cock/core/memory/heap.hpp>
#include <cock/core/memory/vmm.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::core::memory {
Heap kernel_heap;

void Heap::insertFreeBlock(MemoryBlock *block) {
	block->is_free = true;
	if (head == nullptr || head > block) {
		block->next = head;
		head = block;
		return;
	}

	MemoryBlock *current = head;

	while (current->next != nullptr && current->next < block)
		current = current->next;

	block->next = current->next;
	current->next = block;
}

void Heap::removeFreeBlock(MemoryBlock *block, MemoryBlock *prev) {
	if (prev == nullptr) head = block->next;
	else prev->next = block->next;
	block->is_free = false;
}

MemoryBlock *Heap::findFirstAvailable(size_t size, MemoryBlock **out_prev) {
	MemoryBlock *current = head;
	MemoryBlock *prev = nullptr;

	while (current != nullptr) {
		if (current->size >= size) {
			if (out_prev) *out_prev = prev;
			return current;
		}
		prev = current;
		current = current->next;
	}
	if (out_prev) *out_prev = nullptr;
	return nullptr;
}

void Heap::coalesce() {
	MemoryBlock *current = head;
	while (current != nullptr && current->next != nullptr) {
		uintptr_t current_end = reinterpret_cast<uintptr_t>(current) +
								sizeof(MemoryBlock) + current->size;
		if (current_end == reinterpret_cast<uintptr_t>(current->next)) {
			current->size += sizeof(MemoryBlock) + current->next->size;
			current->next = current->next->next;
		} else current = current->next;
	}
}

void Heap::split(MemoryBlock *block, size_t needed_size) {
	if (block->size > needed_size + sizeof(MemoryBlock) + ALIGNMENT) {
		MemoryBlock *new_block =
			reinterpret_cast<MemoryBlock *>(reinterpret_cast<uint8_t *>(block) +
											sizeof(MemoryBlock) + needed_size);
		new_block->size = block->size - needed_size - sizeof(MemoryBlock);
		new_block->is_free = true;

		new_block->next = block->next;
		block->next = new_block;
		block->size = needed_size;
	}
}

void *Heap::allocate(size_t size) {
	if (size == 0) return nullptr;
	size_t aligned_size = alignUp(size);
	MemoryBlock *prev = nullptr;
	MemoryBlock *block = findFirstAvailable(aligned_size, &prev);

	if (block == nullptr) {
		size_t total_needed = sizeof(MemoryBlock) + aligned_size;
		size_t pages = (total_needed + PAGE_SIZE - 1) / PAGE_SIZE;

		void *new_pages = VirtualMemoryManager::allocPages(pages);
		if (!new_pages) return nullptr;

		block = reinterpret_cast<MemoryBlock *>(new_pages);
		block->size = (pages * PAGE_SIZE) - sizeof(MemoryBlock);

		insertFreeBlock(block);

		block = findFirstAvailable(aligned_size, &prev);
	}
	split(block, aligned_size);
	removeFreeBlock(block, prev);

	return reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(block) +
									sizeof(MemoryBlock));
}

void Heap::free(void *ptr) {
	if (ptr == nullptr) return;
	MemoryBlock *block = reinterpret_cast<MemoryBlock *>(
		reinterpret_cast<uint8_t *>(ptr) - sizeof(MemoryBlock));
	if (block->is_free) return;
	insertFreeBlock(block);
	coalesce();
}

void *Heap::callocate(size_t num, size_t size) {
	size_t total_size = num * size;
	void *ptr = allocate(total_size);
	if (ptr != nullptr) memset(ptr, 0, total_size);
	return ptr;
}

void *Heap::reallocate(void *ptr, size_t size) {
	if (ptr == nullptr) return allocate(size);

	if (size == 0) {
		free(ptr);
		return nullptr;
	}

	MemoryBlock *block = reinterpret_cast<MemoryBlock *>(
		reinterpret_cast<uint8_t *>(ptr) - sizeof(MemoryBlock));
	if (block->size >= size) return ptr;

	void *new_ptr = allocate(size);
	if (new_ptr != nullptr) {
		memcpy(new_ptr, ptr, block->size);
		free(ptr);
	}

	return new_ptr;
}

void *kmalloc(size_t size) { return kernel_heap.allocate(size); }

void kfree(void *ptr) { return kernel_heap.free(ptr); }

void *kcalloc(size_t num, size_t size) {
	return kernel_heap.callocate(num, size);
}

void *krealloc(void *ptr, size_t size) {
	return kernel_heap.reallocate(ptr, size);
}

} // namespace cock::core::memory
