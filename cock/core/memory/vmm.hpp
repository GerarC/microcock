#ifndef CORE_VIRT_MM_HPP
#define CORE_VIRT_MM_HPP
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

/**
 * @brief Defines access control and execution permissions for virtual memory
 * pages.
 */
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

/**
 * @brief Architecture-agnostic Virtual Memory Manager.
 * * Handles page allocations, mapping virtual addresses to physical frames, and
 * memory protection.
 */
class VirtualMemoryManager {
  public:
	/**
	 * @brief Allocates contiguous virtual pages and backs them with physical
	 * frames.
	 * @param page_count The number of 4KB pages to allocate.
	 * @param permission Access flags for the allocated pages.
	 * @return A pointer to the start of the newly allocated virtual memory
	 * block.
	 */
	static void *
	allocPages(size_t page_count,
			   VMMPermission permission = VMMPermission::KERNEL_DATA);

	/**
	 * @brief Frees mapped virtual pages and returns physical frames to the PMM.
	 */
	static void freePages(void *vaddr, size_t page_count);

	/**
	 * @brief Maps a specific physical address to a target virtual address.
	 */
	static void map(uintptr_t phys, uintptr_t virt, VMMPermission permission);

	/**
	 * @brief Removes the mapping for a virtual address.
	 */
	static void unmap(uintptr_t virt);

	/**
	 * @brief Clones the Kernel's page directory to create a new isolated User
	 * address space.
	 * @return The physical address of the newly created Page Directory.
	 */
	static uintptr_t createAddressSpace();

	static uintptr_t getKernelDirectory();

	/**
	 * @brief Maps newly allocated physical frames at a specific predefined
	 * virtual address.
	 */
	static void *mapPagesAt(uintptr_t vaddr, size_t num_pages,
							VMMPermission permission);

	/**
	 * @brief Resolves a virtual address to its underlying physical address.
	 * @return The physical address, or 0 if unmapped.
	 */
	static uintptr_t getPhysicalAddress(uintptr_t virt);
};

} // namespace cock::core::memory

#endif // CORE_VIRT_MM_HPP
