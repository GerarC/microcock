#ifndef CORE_PROCESS_HPP
#define CORE_PROCESS_HPP

#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/memory/memory_region.hpp>
#include <cock/core/security/capability.hpp>
#include <cock/data_structure/ll.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

using cock::core::memory::MemoryRegion;
using cock::data_structure::LinkedList;

/**
 * @brief Owns what threads of the same program share: the address space, its
 * user space limit, the hardware permissions and the dynamic mappings.
 * * The process id is the id of its first thread, so IPC to a process reaches
 * its main thread.
 */
class Process {
  public:
	static constexpr size_t MAX_NAME_LENGTH = 32;

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

	memory::VirtAddr getMmapCursor() const { return mmapCursor; }
	void setMmapCursor(memory::VirtAddr cursor) { mmapCursor = cursor; }

	void addMemoryRegion(memory::VirtAddr addr, size_t pages);
	bool takeMemoryRegion(memory::VirtAddr addr, size_t pages);

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
	memory::VirtAddr mmapCursor;
	LinkedList<MemoryRegion> mappedRegions;
};

} // namespace cock::core::task

#endif // !CORE_PROCESS_HPP
