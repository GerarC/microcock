#ifndef CORE_VIRT_MM_HPP
#define CORE_VIRT_MM_HPP
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

// Memory Permissions
enum class VMMPermission : uint32_t {
	NONE = 0,
	READ = 1 << 0,
	WRITE = 1 << 1,
	EXEC = 1 << 2,
	USER = 1 << 3,

	KERNEL_DATA = READ | WRITE,
	USER_DATA = READ | WRITE | USER,
	USER_CODE = READ | EXEC | USER,
};

inline VMMPermission operator|(VMMPermission a, VMMPermission b) {
	return static_cast<VMMPermission>(static_cast<uint32_t>(a) |
									  static_cast<uint32_t>(b));
};

inline bool operator&(VMMPermission a, VMMPermission b) {
	return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
};

class VirtualMemoryManager {
  public:
	static void *
	allocPages(size_t page_count,
			   VMMPermission permission = VMMPermission::KERNEL_DATA);
	static void freePages(void *vaddr, size_t page_count);
	static void map(uintptr_t phys, uintptr_t virt, VMMPermission permission);
	static void unmap(uintptr_t virt);
	static uintptr_t createAddressSpace();
	static uintptr_t getKernelDirectory();
    static void* mapPagesAt(uintptr_t vaddr, size_t num_pages, VMMPermission permission);
};

} // namespace cock::core::memory

#endif // CORE_VIRT_MM_HPP
