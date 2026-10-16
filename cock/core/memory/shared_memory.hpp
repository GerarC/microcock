/** @file
 * @brief Table of shared memory objects: page sets that several processes can
 * map, plus fixed objects that expose boot modules.
 */
#ifndef CORE_SHARED_MEMORY_HPP
#define CORE_SHARED_MEMORY_HPP

#include <cock/abi/shm.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/memory/memory_region.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {
class Process;
}

namespace cock::core::memory {

/**
 * @brief Global registry of shared memory objects.
 *
 * An object is identified by a small id (slot index plus one, never
 * NO_SHARED_OBJECT). Access is by holder: a process may map an object only
 * after the creator or another writable holder shared it. Dynamic objects
 * own their frames and charge their pages to the creating process. Fixed
 * objects alias boot module memory, are never freed and are found by name.
 * All state is static and every operation blocks interrupts while it runs.
 * An object is freed when it has no holder and no mapping left.
 */
class SharedMemory {
  public:
	/** @brief Capacity of the object table. */
	static constexpr size_t MAX_OBJECTS = 32;
	/** @brief Largest object in pages, defined by the user ABI. */
	static constexpr size_t MAX_OBJECT_PAGES = abi::shm::MAX_OBJECT_PAGES;
	/** @brief Processes that can hold one object at the same time. */
	static constexpr size_t MAX_HOLDERS = 8;
	/** @brief Size of the name buffer of a fixed object, terminator included. */
	static constexpr size_t MAX_NAME_LENGTH = 32;

	/**
	 * @brief Creates a dynamic object backed by fresh frames.
	 * @param creator Process that pays for the pages and becomes the first
	 * holder, with write access.
	 * @param pages Size in pages, from 1 to MAX_OBJECT_PAGES.
	 * @return The object id, or NO_SHARED_OBJECT when the size is invalid, the
	 * table is full, the creator is over quota or frames run out.
	 */
	static uint32_t create(task::Process &creator, size_t pages);
	/**
	 * @brief Gives another process access to an object.
	 * @param id Object id.
	 * @param from_pid Process giving the access, must be a holder.
	 * @param target_pid Process that receives it.
	 * @param writable True to grant write access, which the giver must have.
	 * @return True on success. Sharing again can only widen existing access.
	 * False when the object or giver is unknown, the giver lacks write access
	 * it tries to pass, or the holder table is full.
	 */
	static bool share(uint32_t id, uint32_t from_pid, uint32_t target_pid,
					  bool writable);
	/**
	 * @brief Registers a fixed object over contiguous boot module frames.
	 * @param name Unique name, 1 to MAX_NAME_LENGTH minus one characters.
	 * @param module_index Index of the module in the boot list.
	 * @param base Page aligned physical start, not null.
	 * @param pages Size in pages, from 1 to MAX_OBJECT_PAGES.
	 * @return The object id, or NO_SHARED_OBJECT on a bad argument, a
	 * duplicate name or a full table.
	 */
	static uint32_t createFixed(const char *name, size_t module_index,
								PhysAddr base, size_t pages);
	/**
	 * @brief Looks a fixed object up by name.
	 * @param name Name given to createFixed.
	 * @param module_index Receives the boot module index on success.
	 * @return The object id, or NO_SHARED_OBJECT when null arguments are
	 * passed or no such fixed object exists.
	 */
	static uint32_t findModule(const char *name, size_t *module_index);
	/**
	 * @brief Makes a process a read only holder of a fixed object.
	 * @param id Object id, must be a fixed object.
	 * @param pid Process to attach.
	 * @return True when attached or already a holder. False for a dynamic or
	 * unknown object or a full holder table.
	 * @note The caller is responsible for deciding who may attach.
	 */
	static bool attach(uint32_t id, uint32_t pid);
	/**
	 * @brief Drops the access of a process and frees the object if unused.
	 * @param id Object id.
	 * @param pid Holder to remove.
	 * @return True when the process was a holder, false otherwise.
	 */
	static bool release(uint32_t id, uint32_t pid);
	/**
	 * @brief Removes a dying process from every object it holds or pays for.
	 * @param process Process being destroyed.
	 * @note Objects left with no holder and no mapping are freed. The payer
	 * link is cleared so no quota is returned to a dead process.
	 */
	static void dropProcess(task::Process &process);

	/**
	 * @brief Tells whether a process may map an object.
	 * @param id Object id.
	 * @param pid Process asking.
	 * @param writable True to ask for a writable mapping.
	 * @return True when the process holds the object and, for a writable
	 * request, holds it with write access.
	 */
	static bool mayMap(uint32_t id, uint32_t pid, bool writable);
	/**
	 * @brief Size of an object.
	 * @param id Object id.
	 * @return Pages in the object, or 0 for an unknown id.
	 */
	static size_t pageCount(uint32_t id);
	/**
	 * @brief Physical frame of one page of an object.
	 * @param id Object id.
	 * @param index Page index, below pageCount.
	 * @return The frame, or a null address for an unknown id or index.
	 */
	static PhysAddr frame(uint32_t id, size_t index);
	/**
	 * @brief Counts one more mapping of an object, which keeps it alive.
	 * @param id Object id, ignored when unknown.
	 */
	static void mapped(uint32_t id);
	/**
	 * @brief Counts one mapping less and frees the object if it became unused.
	 * @param id Object id, ignored when unknown or without mappings.
	 */
	static void unmapped(uint32_t id);

	/** @return Number of objects currently in use, mostly for tests. */
	static size_t objectCount();

  private:
	/** @brief One process allowed to map an object. */
	struct Holder {
		bool used; /**< Slot in use. */
		bool writable; /**< May map the object writable. */
		uint32_t pid; /**< Process id of the holder. */
	};

	/** @brief One entry of the object table. */
	struct Object {
		bool used; /**< Entry in use. */
		bool fixed; /**< Aliases a boot module, never freed. */
		size_t moduleIndex; /**< Boot module index, for fixed objects. */
		char name[MAX_NAME_LENGTH]; /**< Name of a fixed object. */
		task::Process *payer; /**< Process charged for the pages, null once it
								 died or for fixed objects. */
		size_t pages; /**< Size in pages. */
		size_t mappings; /**< Live mappings in any address space. */
		Holder holders[MAX_HOLDERS]; /**< Processes allowed to map. */
		PhysAddr frames[MAX_OBJECT_PAGES]; /**< Frame of every page. */
	};

	/**
	 * @brief Finds the entry of an id.
	 * @param id Object id, untrusted.
	 * @return The entry, or nullptr when the id is zero, out of range or free.
	 */
	static Object *lookup(uint32_t id);
	/**
	 * @brief Finds the holder slot of a process.
	 * @param object Object to search.
	 * @param pid Process id.
	 * @return The slot, or nullptr when the process is not a holder.
	 */
	static Holder *findHolder(Object &object, uint32_t pid);
	/**
	 * @brief Frees a dynamic object that has no holder and no mapping.
	 * @param object Object to test. Its frames are returned and the payer
	 * gets its quota back.
	 */
	static void freeIfUnused(Object &object);

	static Object objects[MAX_OBJECTS]; /**< The object table. */
};

} // namespace cock::core::memory

#endif // !CORE_SHARED_MEMORY_HPP
