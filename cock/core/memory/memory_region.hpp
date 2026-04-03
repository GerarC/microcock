#ifndef CORE_MEMORY_REGION_HPP
#define CORE_MEMORY_REGION_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

typedef struct memory_region_t {
	uintptr_t virt_addr;
	size_t page_count;

	bool operator==(const memory_region_t &other) const {
		return virt_addr == other.virt_addr;
	}
} MemoryRegion;

} // namespace cock::core::memory

#endif // !CORE_MEMORY_REGION_HPP
