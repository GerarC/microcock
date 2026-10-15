#ifndef CORE_SLAB_POOL_HPP
#define CORE_SLAB_POOL_HPP

#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/memory/vmm.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

template <typename T, size_t CHUNK_PAGES> class SlabPool {
  public:
	static void *allocate() {
		hal::InterruptGuard guard;
		if (freeList == nullptr && !grow()) return nullptr;

		Slot *slot = freeList;
		freeList = slot->next;
		live++;
		return slot;
	}

	static void release(void *pointer) {
		if (pointer == nullptr) return;

		hal::InterruptGuard guard;
		Slot *slot = static_cast<Slot *>(pointer);
		slot->next = freeList;
		freeList = slot;
		live--;
	}

	static size_t inUse() { return live; }
	static size_t chunks() { return chunkCount; }

  private:
	struct Slot {
		Slot *next;
	};

	static_assert(sizeof(T) >= sizeof(Slot), "a slot must hold the free list link");
	static_assert(sizeof(T) % alignof(T) == 0, "slots must keep the alignment of T");
	static_assert(CHUNK_PAGES * hal::PAGE_SIZE >= sizeof(T), "a chunk must hold one object");

	static constexpr size_t SLOTS_PER_CHUNK = CHUNK_PAGES * hal::PAGE_SIZE / sizeof(T);

	inline static Slot *freeList = nullptr;
	inline static size_t live = 0;
	inline static size_t chunkCount = 0;

	static bool grow() {
		uint8_t *chunk = static_cast<uint8_t *>(VirtualMemoryManager::allocPages(CHUNK_PAGES));
		if (chunk == nullptr) return false;

		for (size_t index = SLOTS_PER_CHUNK; index > 0; index--) {
			Slot *slot = reinterpret_cast<Slot *>(chunk + (index - 1) * sizeof(T));
			slot->next = freeList;
			freeList = slot;
		}
		chunkCount++;
		return true;
	}
};

} // namespace cock::core::memory

#endif // !CORE_SLAB_POOL_HPP
