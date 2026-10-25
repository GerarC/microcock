/** @file
 * @brief Architecture agnostic virtual memory manager interface: kernel page
 * allocation, address space creation, user range validation and mapping.
 *
 * The implementation lives in the HAL side of the tree. Unless said
 * otherwise, operations that change mappings block interrupts while they run.
 */
#ifndef CORE_VIRT_MM_HPP
#define CORE_VIRT_MM_HPP
#include <cock/core/memory/address.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

/** @brief First address user code may not access, as a raw integer. */
constexpr uintptr_t USER_SPACE_LIMIT = hal::USER_SPACE_LIMIT;
/** @brief First address user code may not access, as a VirtAddr. */
constexpr VirtAddr USER_SPACE_END = USER_SPACE_LIMIT_ADDRESS;

/**
 * @brief Defines access control and execution permissions for virtual memory
 * pages.
 *
 * Values are bit flags that can be combined with the | operator and tested
 * with the & operator. The composite values are the usual combinations.
 */
enum class VMMPermission : uint32_t {
	NONE = 0, /**< No access. */
	READ = 1 << 0, /**< Readable. */
	WRITE = 1 << 1, /**< Writable. */
	EXEC = 1 << 2, /**< Executable, where the architecture supports it. */
	USER = 1 << 3, /**< Accessible from Ring 3. */
	DEVICE = 1 << 4, /**< Device memory: uncached and not owned, the frame is
						never returned to the physical allocator. */
	SHARED = 1 << 5, /**< Frame owned by someone else, so it is not freed when
						unmapped. */

	KERNEL_DATA = READ | WRITE, /**< Kernel read and write data. */
	USER_DATA = READ | WRITE | USER, /**< User read and write data. */
	USER_CODE = READ | EXEC | USER, /**< User read only executable code. */
	USER_READONLY = READ | USER, /**< User read only data. */
};

/**
 * @brief Combines two permission sets.
 * @param a First set.
 * @param b Second set.
 * @return The union of the flags.
 */
inline VMMPermission operator|(VMMPermission a, VMMPermission b) {
	return static_cast<VMMPermission>(static_cast<uint32_t>(a) |
									  static_cast<uint32_t>(b));
};

/**
 * @brief Tests whether two permission sets share a flag.
 * @param a First set.
 * @param b Second set, normally a single flag.
 * @return True when at least one flag is in both.
 */
inline bool operator&(VMMPermission a, VMMPermission b) {
	return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
};

/**
 * @brief Architecture-agnostic Virtual Memory Manager.
 * * Handles page allocations, mapping virtual addresses to physical frames, and
 * memory protection.
 *
 * All members are static. Newly allocated frames are always zeroed.
 */
class VirtualMemoryManager {
  public:
	/**
	 * @brief Allocates contiguous virtual pages and backs them with physical
	 * frames.
	 * @param page_count The number of 4KB pages to allocate.
	 * @param permission Access flags for the allocated pages. Must not
	 * include USER.
	 * @return A pointer to the start of the newly allocated virtual memory
	 * block, zero filled, or nullptr when the count is zero, USER is asked, the
	 * kernel range is exhausted or frames run out (a fatal message is
	 * logged).
	 * @note Kernel address space only.
	 */
	static void *
	allocPages(size_t page_count,
			   VMMPermission permission = VMMPermission::KERNEL_DATA);

	/**
	 * @brief Frees mapped virtual pages and returns physical frames to the PMM.
	 * @param vaddr Start of a block from allocPages.
	 * @param page_count Number of pages of that block.
	 * @note Frames mapped as device or shared are unmapped but not freed.
	 */
	static void freePages(void *vaddr, size_t page_count);
	/**
	 * @brief Allocates a kernel stack with an unmapped guard page below it.
	 * @param page_count Stack size in pages, not counting the guard.
	 * @return The lowest address of the usable stack, or nullptr when the
	 * count is zero or memory is exhausted.
	 */
	static void *allocStack(size_t page_count);
	/**
	 * @brief Frees a stack made by allocStack, including its guard page.
	 * @param base Pointer returned by allocStack, null is ignored.
	 * @param page_count The same page count given to allocStack.
	 */
	static void freeStack(void *base, size_t page_count);
	/** @return The end of the kernel virtual range handed out so far. */
	static uintptr_t kernelRangeEnd();

	/**
	 * @brief Gives back the whole pages of a physical range that the boot
	 * reserved (a boot module that was already copied). A partial page at
	 * either end stays reserved.
	 * @param start Physical start of the range.
	 * @param bytes Length of the range in bytes.
	 */
	static void releaseBootFrames(PhysAddr start, size_t bytes);

	/**
	 * @brief Maps a specific physical address to a target virtual address.
	 * @param phys Physical frame to map.
	 * @param virt Virtual page, in the active address space.
	 * @param permission Access flags.
	 * @return True when the mapping was made, false when the page tables could
	 * not be extended.
	 */
	static bool map(PhysAddr phys, VirtAddr virt, VMMPermission permission);

	/**
	 * @brief Removes the mapping for a virtual address.
	 * @param virt Virtual page in the active address space.
	 * @note Does not free the frame, the caller owns it.
	 */
	static void unmap(VirtAddr virt);

	/**
	 * @brief Clones the Kernel's page directory to create a new isolated User
	 * address space.
	 * @return The physical address of the newly created Page Directory, null
	 * on failure.
	 */
	static PhysAddr createAddressSpace();
	/**
	 * @brief Tears down a user address space and drops its hold on the frames it
	 * owns.
	 * @param address_space Root returned by createAddressSpace or
	 * cloneAddressSpace, not the one in use.
	 * @note A frame shared copy on write goes back to the allocator with its
	 * last holder.
	 */
	static void destroyAddressSpace(PhysAddr address_space);

	/**
	 * @brief Copies a user address space without copying its pages: both spaces
	 * map the same frames, and a page that was writable becomes copy on write in
	 * both, so the first write on either side gets a private frame
	 * (resolveCopyOnWrite). Pages of shared memory and devices stay shared as
	 * they are.
	 * @param source Root of a user address space that is not the one in use.
	 * @return The root of the copy, or a null address when the source is not a
	 * user address space or is the active one, free frames would fall below the
	 * kernel reserve, or a frame has too many references. Nothing stays allocated
	 * after a failure.
	 * @note Only the directory and the page tables take frames now; the frames
	 * for the copies are taken one by one at the writes, and a write that finds
	 * none free ends the thread as a fault.
	 */
	static PhysAddr cloneAddressSpace(PhysAddr source);

	/**
	 * @brief Gives the active address space a private writable frame for a copy
	 * on write page. Called by the fault handler for a write to a present page.
	 * @param virt Faulting address, untrusted.
	 * @return True when the page is writable now and the access can be repeated.
	 * False when the address is not a user address, the page is not present or
	 * was never copy on write (a real protection violation), or no frame is
	 * free.
	 */
	static bool resolveCopyOnWrite(VirtAddr virt);

	/** @return The root of the kernel address space. */
	static PhysAddr getKernelDirectory();

	/**
	 * @brief Maps newly allocated physical frames at a specific predefined
	 * virtual address.
	 * @param vaddr Start address, rounded down to its page.
	 * @param num_pages Number of pages, non zero.
	 * @param permission Access flags. With USER the range must end at or below
	 * @p user_limit.
	 * @param user_limit Upper bound for user mappings, non null and not above
	 * USER_SPACE_END.
	 * @return The page aligned start on success, a null address on failure:
	 * zero pages, overflow, range above the limit, not enough free frames
	 * (a reserve is kept for the kernel on user mappings) or out of memory.
	 * @note Pages that are already mapped keep their frame and gain the new
	 * flags. Works on the active address space. Frames are zeroed.
	 */
	static VirtAddr mapPagesAt(VirtAddr vaddr, size_t num_pages,
							   VMMPermission permission, VirtAddr user_limit);

	/**
	 * @brief Replaces the permissions of an already mapped page.
	 * @param virt Mapped page in the active address space.
	 * @param permission New permissions.
	 * @return True on success, false when the page is not mapped.
	 */
	static bool protectPage(VirtAddr virt, VMMPermission permission);

	/**
	 * @brief Checks that a user supplied range may be accessed.
	 * @param ptr Start of the range, untrusted.
	 * @param length Length in bytes.
	 * @param write True when the kernel is going to write to it.
	 * @param user_limit Upper bound of the process user space.
	 * @return True when the range lies below the limit without overflow and
	 * every page is mapped, user accessible and writable if asked. False
	 * otherwise. This is the check every syscall must run before touching a
	 * user pointer.
	 */
	static bool isUserRangeValid(VirtAddr ptr, size_t length, bool write,
								 VirtAddr user_limit);

	/**
	 * @brief Resolves a virtual address to its underlying physical address.
	 * @param virt Virtual address in the active address space.
	 * @return The physical address, or 0 if unmapped.
	 */
	static PhysAddr getPhysicalAddress(VirtAddr virt);

	/**
	 * @brief Tells whether a physical range is device memory, not RAM.
	 * @param phys Start of the range.
	 * @param length Length in bytes.
	 * @return True when no frame of the range is RAM. False for an empty or
	 * overflowing range.
	 */
	static bool isDeviceMemory(PhysAddr phys, size_t length);

	/**
	 * @brief Tells whether no page of a range is mapped.
	 * @param start First page of the range.
	 * @param page_count Number of pages.
	 * @return True when every page is unmapped.
	 */
	static bool isRangeUnmapped(VirtAddr start, size_t page_count);

	/**
	 * @brief Allocates zeroed physical frames without mapping them.
	 * @param frames Output array of @p count entries.
	 * @param count Number of frames, non zero.
	 * @return True when all were allocated. False for a null array, a zero
	 * count, too few free frames (a reserve is kept for the kernel) or
	 * failure, in which case nothing stays allocated.
	 */
	static bool allocFrames(PhysAddr *frames, size_t count);
	/**
	 * @brief Returns frames to the physical allocator.
	 * @param frames Array of frames from allocFrames.
	 * @param count Number of entries.
	 */
	static void freeFrames(const PhysAddr *frames, size_t count);
};

} // namespace cock::core::memory

#endif // CORE_VIRT_MM_HPP
