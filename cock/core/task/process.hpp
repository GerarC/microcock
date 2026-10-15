#ifndef CORE_PROCESS_HPP
#define CORE_PROCESS_HPP

#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/memory/memory_region.hpp>
#include <cock/core/security/capability.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

using cock::core::memory::MemoryRegion;

class Process {
  public:
	static constexpr size_t MAX_NAME_LENGTH = 32;
	static constexpr size_t MAX_MAPPING_REGIONS = 32;
	static constexpr size_t MAX_OWNED_PAGES = 0x1000;

	static void *operator new(size_t size);
	static void operator delete(void *pointer);

	static Process *createKernel();
	static Process *create(memory::PhysAddr address_space,
						   memory::VirtAddr user_space_limit);
	~Process();

	Process(const Process &) = delete;
	Process &operator=(const Process &) = delete;

	uint32_t getId() const { return id; }
	bool isKernel() const { return kernel; }
	memory::PhysAddr getAddressSpace() const { return addressSpace; }
	memory::VirtAddr getUserSpaceLimit() const { return userSpaceLimit; }

	hal::ArchProcessContext &getContext() { return context; }
	const hal::ArchProcessContext &getContext() const { return context; }

	security::CapabilitySet &getCapabilities() { return capabilities; }
	const security::CapabilitySet &getCapabilities() const {
		return capabilities;
	}

	void setName(const char *new_name);
	const char *getName() const { return name; }

	size_t getThreadCount() const { return threadCount; }
	void attachThread(uint32_t thread_id);
	bool detachThread();

	bool hasRegionSlot() const { return regionCount < MAX_MAPPING_REGIONS; }
	bool canOwnPages(size_t pages) const {
		return pages <= MAX_OWNED_PAGES - ownedPages;
	}
	size_t getOwnedPages() const { return ownedPages; }
	size_t getRegionCount() const { return regionCount; }

	bool reservePages(size_t pages);
	void returnPages(size_t pages);
	bool addMemoryRegion(memory::VirtAddr addr, size_t pages, bool owned,
						 uint32_t shared_object = memory::NO_SHARED_OBJECT);
	bool takeMemoryRegion(memory::VirtAddr addr, size_t pages,
						  uint32_t *shared_object = nullptr);
	memory::VirtAddr findFreeRange(size_t pages, memory::VirtAddr from,
								   memory::VirtAddr limit) const;

  private:
	Process(memory::PhysAddr address_space, memory::VirtAddr user_space_limit,
			bool is_kernel);

	uint32_t id = 0;
	bool kernel;
	memory::PhysAddr addressSpace;
	memory::VirtAddr userSpaceLimit;
	hal::ArchProcessContext context;
	security::CapabilitySet capabilities;
	char name[MAX_NAME_LENGTH] = {};
	size_t threadCount = 0;
	MemoryRegion regions[MAX_MAPPING_REGIONS] = {};
	size_t regionCount = 0;
	size_t ownedPages = 0;
};

} // namespace cock::core::task

#endif // !CORE_PROCESS_HPP
