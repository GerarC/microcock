#ifndef CORE_SHARED_MEMORY_HPP
#define CORE_SHARED_MEMORY_HPP

#include <cock/core/memory/address.hpp>
#include <cock/core/memory/memory_region.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {
class Process;
}

namespace cock::core::memory {

class SharedMemory {
  public:
	static constexpr size_t MAX_OBJECTS = 32;
	static constexpr size_t MAX_OBJECT_PAGES = 128;
	static constexpr size_t MAX_HOLDERS = 8;

	static uint32_t create(task::Process &creator, size_t pages);
	static bool share(uint32_t id, uint32_t from_pid, uint32_t target_pid,
					  bool writable);
	static bool release(uint32_t id, uint32_t pid);
	static void dropProcess(task::Process &process);

	static bool mayMap(uint32_t id, uint32_t pid, bool writable);
	static size_t pageCount(uint32_t id);
	static PhysAddr frame(uint32_t id, size_t index);
	static void mapped(uint32_t id);
	static void unmapped(uint32_t id);

	static size_t objectCount();

  private:
	struct Holder {
		bool used;
		bool writable;
		uint32_t pid;
	};

	struct Object {
		bool used;
		task::Process *payer;
		size_t pages;
		size_t mappings;
		Holder holders[MAX_HOLDERS];
		PhysAddr frames[MAX_OBJECT_PAGES];
	};

	static Object *lookup(uint32_t id);
	static Holder *findHolder(Object &object, uint32_t pid);
	static void freeIfUnused(Object &object);

	static Object objects[MAX_OBJECTS];
};

} // namespace cock::core::memory

#endif // !CORE_SHARED_MEMORY_HPP
