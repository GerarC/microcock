/** @file
 * @brief Fixed size object pool that grows by whole chunks of pages.
 */
#ifndef CORE_SLAB_POOL_HPP
#define CORE_SLAB_POOL_HPP

#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/memory/vmm.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

/**
 * @brief Static pool of raw slots for objects of type T.
 *
 * Slots are carved out of chunks of CHUNK_PAGES pages taken from the virtual
 * memory manager and recycled through an intrusive free list. Allocation and
 * release are constant time. The pool only hands out raw memory: the caller
 * constructs and destroys the object. Chunks are never given back. One
 * instance of the state exists per template instantiation.
 *
 * @tparam T Type stored in each slot, at least pointer sized.
 * @tparam CHUNK_PAGES Pages requested every time the pool grows.
 */
template <typename T, size_t CHUNK_PAGES> class SlabPool {
  public:
	/**
	 * @brief Takes a slot from the pool, growing it when empty.
	 * @return Uninitialized memory for one T, or nullptr when no chunk can be
	 * allocated.
	 * @note Blocks interrupts while it runs.
	 */
	static void *allocate() {
		hal::InterruptGuard guard;
		if (freeList == nullptr && !grow()) return nullptr;

		Slot *slot = freeList;
		freeList = slot->next;
		live++;
		return slot;
	}

	/**
	 * @brief Returns a slot to the pool.
	 * @param pointer Slot obtained from allocate, or null which is ignored.
	 * @pre The object in the slot has been destroyed and the slot is not
	 * released twice.
	 * @note Blocks interrupts while it runs.
	 */
	static void release(void *pointer) {
		if (pointer == nullptr) return;

		hal::InterruptGuard guard;
		Slot *slot = static_cast<Slot *>(pointer);
		slot->next = freeList;
		freeList = slot;
		live--;
	}

	/** @return Number of slots currently handed out. */
	static size_t inUse() { return live; }
	/** @return Number of chunks the pool has grown by. */
	static size_t chunks() { return chunkCount; }

  private:
	/** @brief Overlay of a free slot: only the free list link. */
	struct Slot {
		Slot *next; /**< Next free slot. */
	};

	static_assert(sizeof(T) >= sizeof(Slot), "a slot must hold the free list link");
	static_assert(sizeof(T) % alignof(T) == 0, "slots must keep the alignment of T");
	static_assert(CHUNK_PAGES * hal::PAGE_SIZE >= sizeof(T), "a chunk must hold one object");

	/** @brief How many slots fit in one chunk. */
	static constexpr size_t SLOTS_PER_CHUNK = CHUNK_PAGES * hal::PAGE_SIZE / sizeof(T);

	inline static Slot *freeList = nullptr; /**< Head of the free slot list. */
	inline static size_t live = 0; /**< Slots handed out. */
	inline static size_t chunkCount = 0; /**< Chunks allocated so far. */

	/**
	 * @brief Allocates one chunk and threads its slots onto the free list.
	 * @return False when the virtual memory manager is out of memory.
	 * @pre Interrupts are blocked by the caller.
	 */
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
