/** @file
 * @brief Process: an address space with its capabilities, memory regions,
 * quota and the threads that run in it.
 */
#ifndef CORE_PROCESS_HPP
#define CORE_PROCESS_HPP

#include <cock/abi/process.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/memory/memory_region.hpp>
#include <cock/core/security/capability.hpp>
#include <cock/data_structure/ring_buffer.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

using cock::core::memory::MemoryRegion;

/** @brief An exit notice kept for a parent until it reads it. */
struct ExitNotice {
	uint32_t child; /**< Process id of the child that exited. */
	uint32_t status; /**< Its exit status. */
};

/**
 * @brief The resources shared by the threads of one program.
 *
 * A process owns a page directory (except the kernel process, which uses the
 * kernel one), a capability set, a table of mapped regions, a page quota and
 * the architecture state (I/O permissions). Its id is the id of its first
 * thread. Objects live in a slab pool and are destroyed by the last thread
 * that detaches. Not copyable.
 */
class Process {
  public:
	/** @brief Size of the name buffer, terminator included. */
	static constexpr size_t MAX_NAME_LENGTH = 32;
	/** @brief Capacity of the region table, which bounds mmap and shm maps. */
	static constexpr size_t MAX_MAPPING_REGIONS = 32;
	/** @brief Page quota: most pages the process may own or share as payer. */
	static constexpr size_t MAX_OWNED_PAGES = 0x1000;
	/** @brief Most threads a process may have, from the user ABI. */
	static constexpr size_t MAX_THREADS = abi::process::MAX_THREADS_PER_PROCESS;

	/**
	 * @brief Allocates a process from the pool.
	 * @param size Must be sizeof(Process).
	 * @return A slot. Panics on a wrong size or when the pool is exhausted.
	 */
	static void *operator new(size_t size);
	/**
	 * @brief Returns a process slot to the pool.
	 * @param pointer Slot from operator new, null is ignored.
	 */
	static void operator delete(void *pointer);

	/**
	 * @brief Creates the kernel process, which runs in the kernel address
	 * space and has no user limit.
	 * @return The new process.
	 */
	static Process *createKernel();
	/**
	 * @brief Creates a user process.
	 * @param address_space Page directory built by the VMM, now owned by the
	 * process and destroyed with it.
	 * @param user_space_limit First address the process may not use.
	 * @return The new process, without threads.
	 */
	static Process *create(memory::PhysAddr address_space,
						   memory::VirtAddr user_space_limit);
	/**
	 * @brief Releases the architecture state and, for a user process, destroys
	 * its address space, unmaps shared regions and drops its shared objects.
	 */
	~Process();

	/** @brief Copying is forbidden: a process owns an address space. */
	Process(const Process &) = delete;
	/** @brief Assignment is forbidden: a process owns an address space. */
	Process &operator=(const Process &) = delete;

	/** @return The process id, the id of its first thread, 0 before one is
	 * attached. */
	uint32_t getId() const { return id; }
	/** @return True for the kernel process. */
	bool isKernel() const { return kernel; }
	/** @return Physical root of the address space. */
	memory::PhysAddr getAddressSpace() const { return addressSpace; }
	/** @return First address user code may not touch in this process. */
	memory::VirtAddr getUserSpaceLimit() const { return userSpaceLimit; }

	/** @return The architecture state, such as I/O permissions. */
	hal::ArchProcessContext &getContext() { return context; }
	/** @return The architecture state, read only. */
	const hal::ArchProcessContext &getContext() const { return context; }

	/** @return The capabilities of the process, modifiable. */
	security::CapabilitySet &getCapabilities() { return capabilities; }
	/** @return The capabilities of the process, read only. */
	const security::CapabilitySet &getCapabilities() const {
		return capabilities;
	}

	/**
	 * @brief Sets the name used by process lookup.
	 * @param new_name Null terminated string, truncated to fit. A null pointer
	 * gives an empty name.
	 */
	void setName(const char *new_name);
	/** @return The null terminated name. */
	const char *getName() const { return name; }

	/** @return Number of threads attached and not yet reaped. */
	size_t getThreadCount() const { return threadCount; }
	/**
	 * @brief Attaches a new thread. The first one gives the process its id.
	 * @param thread_id Id of the thread.
	 */
	void attachThread(uint32_t thread_id);
	/**
	 * @brief Detaches a reaped thread.
	 * @return True when it was the last one, so the process must be deleted.
	 */
	bool detachThread();

	/** @return Number of threads that have not exited yet. */
	size_t getLiveThreadCount() const { return liveThreads; }
	/** @return True when the thread limit has not been reached. */
	bool canAttachThread() const { return threadCount < MAX_THREADS; }
	/**
	 * @brief Records that a thread exited.
	 * @return True when no live thread is left, which means the process
	 * exited.
	 */
	bool threadExited();

	/** @return Id of the process that spawned this one, or the no parent
	 * value. */
	uint32_t getParentId() const { return parentId; }
	/**
	 * @brief Sets the parent process.
	 * @param parent Parent process id.
	 */
	void setParentId(uint32_t parent) { parentId = parent; }
	/** @return True when the parent wants a message when this process exits. */
	bool notifiesExit() const { return notifyExit; }
	/**
	 * @brief Chooses whether the parent is told about the exit.
	 * @param notify True to send the exit message.
	 */
	void setNotifiesExit(bool notify) { notifyExit = notify; }
	/** @return Status given to exit, or the fault status when killed. */
	uint32_t getExitStatus() const { return exitStatus; }
	/**
	 * @brief Records the exit status.
	 * @param status Value reported to the parent.
	 */
	void setExitStatus(uint32_t status) { exitStatus = status; }

	/** @return True when another child may be spawned with an exit notice. */
	bool canReserveExitNotice() const {
		return notifiedChildren < abi::process::MAX_NOTIFIED_CHILDREN;
	}
	/**
	 * @brief Reserves the record slot of the exit notice of a child about to be
	 * spawned.
	 * @return False when MAX_NOTIFIED_CHILDREN children are already counted,
	 * nothing is reserved then.
	 */
	bool reserveExitNotice();
	/**
	 * @brief Stores the exit notice of a child in the reserved slot.
	 * @param child Process id of the child.
	 * @param status Exit status of the child.
	 * @return False when the record is full, which cannot happen for children
	 * that reserved a slot at spawn.
	 */
	bool recordExitNotice(uint32_t child, uint32_t status);
	/**
	 * @brief Takes the oldest exit notice and gives its slot back.
	 * @param notice Receives the child id and its status.
	 * @return False when no notice is stored.
	 */
	bool takeExitNotice(ExitNotice &notice);
	/** @return True when an exit notice waits to be read. */
	bool hasExitNotices() const { return !exitNotices.isEmpty(); }

	/** @return True when the region table has a free entry. */
	bool hasRegionSlot() const { return regionCount < MAX_MAPPING_REGIONS; }
	/**
	 * @brief Tells whether the quota allows more owned pages.
	 * @param pages Pages wanted.
	 * @return True when they fit under MAX_OWNED_PAGES.
	 */
	bool canOwnPages(size_t pages) const {
		return pages <= MAX_OWNED_PAGES - ownedPages;
	}
	/** @return Pages currently charged to the quota. */
	size_t getOwnedPages() const { return ownedPages; }
	/** @return Number of entries in the region table. */
	size_t getRegionCount() const { return regionCount; }

	/**
	 * @brief Charges pages to the quota without recording a region, as shared
	 * memory does.
	 * @param pages Pages to charge.
	 * @return False when the quota would be exceeded, nothing is charged then.
	 */
	bool reservePages(size_t pages);
	/**
	 * @brief Gives pages back to the quota.
	 * @param pages Pages to return, clamped to what is charged.
	 */
	void returnPages(size_t pages);
	/**
	 * @brief Records a mapped region.
	 * @param addr Start of the region.
	 * @param pages Size in pages, non zero.
	 * @param owned True when the frames belong to the process and count against
	 * its quota.
	 * @param shared_object Shared object id behind the region, or none.
	 * @param device_base Physical address of the first page for a region that maps
	 * device memory, or NO_DEVICE_BASE.
	 * @return False when the table is full, the size is zero or an owned region
	 * exceeds the quota.
	 */
	bool addMemoryRegion(memory::VirtAddr addr, size_t pages, bool owned,
						 uint32_t shared_object = memory::NO_SHARED_OBJECT,
						 uint64_t device_base = memory::NO_DEVICE_BASE);
	/**
	 * @brief Reads one entry of the region table.
	 * @param index Position, below getRegionCount().
	 * @return A copy of the entry. An index out of range gives an empty region
	 * (no pages, no device).
	 */
	memory::MemoryRegion getRegion(size_t index) const {
		if (index >= regionCount) return {0, 0, false, memory::NO_SHARED_OBJECT, memory::NO_DEVICE_BASE};
		return regions[index];
	}
	/**
	 * @brief Removes a region from the table.
	 * @param addr Exact start of the region.
	 * @param pages Exact size in pages.
	 * @param shared_object Receives the shared object id of the region, may be
	 * null.
	 * @return True when a region with that start and size existed. It does not
	 * unmap anything, the caller does.
	 */
	bool takeMemoryRegion(memory::VirtAddr addr, size_t pages,
						  uint32_t *shared_object = nullptr);
	/**
	 * @brief Finds an address range that overlaps no recorded region.
	 * @param pages Size in pages, non zero.
	 * @param from Lowest address to consider.
	 * @param limit First address the range may not reach.
	 * @return The start of the first such range, or a null address when none
	 * fits. It only looks at the table, pages mapped by other means are not
	 * seen.
	 */
	memory::VirtAddr findFreeRange(size_t pages, memory::VirtAddr from,
								   memory::VirtAddr limit) const;

  private:
	/**
	 * @brief Builds a process and initializes its architecture state.
	 * @param address_space Page directory.
	 * @param user_space_limit User space bound.
	 * @param is_kernel True for the kernel process.
	 */
	Process(memory::PhysAddr address_space, memory::VirtAddr user_space_limit,
			bool is_kernel);

	uint32_t id = 0; /**< Id of the first thread. */
	bool kernel; /**< True for the kernel process. */
	memory::PhysAddr addressSpace; /**< Page directory root. */
	memory::VirtAddr userSpaceLimit; /**< User space bound. */
	hal::ArchProcessContext context; /**< Architecture state. */
	security::CapabilitySet capabilities; /**< Held capabilities. */
	char name[MAX_NAME_LENGTH] = {}; /**< Name for lookup. */
	size_t threadCount = 0; /**< Attached threads. */
	size_t liveThreads = 0; /**< Threads that have not exited. */
	uint32_t parentId = abi::process::NO_PARENT; /**< Spawning process. */
	bool notifyExit = false; /**< Tell the parent on exit. */
	uint32_t exitStatus = 0; /**< Exit status. */
	size_t notifiedChildren = 0; /**< Children with an exit notice that is reserved or stored. */
	data_structure::RingBuffer<ExitNotice, abi::process::MAX_NOTIFIED_CHILDREN + 1>
		exitNotices; /**< Exit notices of children, not yet read. */
	MemoryRegion regions[MAX_MAPPING_REGIONS] = {}; /**< Mapped regions. */
	size_t regionCount = 0; /**< Entries used in regions. */
	size_t ownedPages = 0; /**< Pages charged to the quota. */
};

} // namespace cock::core::task

#endif // !CORE_PROCESS_HPP
