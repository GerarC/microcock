/** @file
 * @brief Record of a region of virtual memory a process has mapped.
 */
#ifndef CORE_MEMORY_REGION_HPP
#define CORE_MEMORY_REGION_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

/** @brief Value of MemoryRegion::shared_object for a region that is not backed
 * by a shared memory object. */
constexpr uint32_t NO_SHARED_OBJECT = 0;

/**
 * @brief Tracks dynamically allocated virtual memory pages for a process.
 * * Used by the Thread class for garbage collection and memory leak prevention.
 */
typedef struct memory_region_t {
	uintptr_t virt_addr;        /**< Starting virtual address of the allocated region. */
	size_t page_count;          /**< Number of contiguous 4KB pages allocated. */
	bool owned;                 /**< True when the frames belong to the process and count against its quota. */
	uint32_t shared_object;     /**< Id of the shared memory object that backs the
									region, or NO_SHARED_OBJECT for private memory. */
} MemoryRegion;

} // namespace cock::core::memory

#endif // !CORE_MEMORY_REGION_HPP
