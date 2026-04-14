#ifndef ARCH_X86_MEMORY_HPP
#define ARCH_X86_MEMORY_HPP
#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

static constexpr size_t PAGE_DIR_SIZE = 0x400;
static constexpr size_t FRAME_SIZE = 0x0020;
static constexpr uint32_t DIRECTORY_MASK = 0x03FF;
static constexpr uint32_t TABLE_MASK = 0xFFF;
static constexpr uint32_t PAGE_DIRECTORY_IDX_OFFSET = 0x16;
static constexpr uint32_t PAGE_TABLE_IDX_OFFSET = 0x0C;
static constexpr uint32_t NULL_PTR = 0x0;
static constexpr uint32_t LAST_ADDRESS = 0xFFFFFFFF;
static constexpr uint32_t PAGE_SIZE = 0x1000;

/**
 * @brief Architecture-specific Physical Memory Manager (x86).
 * * Manages the allocation and deallocation of physical 4KB memory frames using
 * a bitmap approach.
 */
class PhysicalMemoryManager {
  public:
	/**
	 * @brief Initializes the physical memory manager using the Multiboot memory
	 * map.
	 * @param boot_info Pointer to the multiboot info structure provided by
	 * GRUB.
	 */
	static void init(MBInfo *boot_info);

	static void pmmInit(uint32_t mem_low, uint32_t mem_high);

	/**
	 * @brief Invalidates a specific page in the Translation Lookaside Buffer
	 * (TLB).
	 * @param virtual_address The virtual address whose cached translation
	 * should be flushed.
	 */
	static void invalidatePage(uint32_t virtual_address);

	/**
	 * @brief Allocates the first available physical memory frame.
	 * @return The physical address of the allocated frame, or LAST_ADDRESS if
	 * out of memory.
	 */
	static uint32_t allocFrame();

	/**
	 * @brief Marks a physical frame as free in the bitmap.
	 * @param phys_addr The physical address of the frame to free.
	 */
	static void freeFrame(uint32_t phys_addr);

	/**
	 * @brief Maps a physical frame to a virtual address in the current page
	 * directory.
	 * @param phys The physical address to map.
	 * @param virt The target virtual address.
	 * @param flags The access and permission flags for the page table entry.
	 */
	static void mapPage(uint32_t phys, uint32_t virt, uint32_t flags);

	/**
	 * @brief Unmaps a mapped virtual address, clearing its page table entry.
	 * @param virt The virtual address to unmap.
	 */
	static void unmapPage(uint32_t virt);

	/**
	 * @brief Resolves a virtual address to its corresponding physical frame
	 * address.
	 * @param virt The virtual address to resolve.
	 * @return The underlying physical address, or NULL_PTR if it is not mapped.
	 */
	static uint32_t getPhysicalAddress(uint32_t virt);

	/**
	 * @brief Creates a completely new Page Directory (Address Space) for a user
	 * process.
	 * @return The physical address of the new Page Directory.
	 */
	static uint32_t createAddressSpace();

	/**
	 * @brief Retrieves the physical address of the base Kernel Page Directory.
	 * @return The physical address of the kernel's directory.
	 */
	static uint32_t getKernelDirectory();

  private:
	static constexpr uint32_t PAGE_SIZE = 0x1000;
	static constexpr uint32_t NUM_PAGE_DIRS = 0x100;
	static constexpr uint32_t NUM_PAGE_FRAMES =
		(0x100000000ULL / PAGE_SIZE / 8);

	static constexpr uintptr_t KERNEL_START = 0xC0000000;

	static MBInfo *bootInfo;
	static uint32_t pageFrameMin;
	static uint32_t pageFrameMax;
	static uint32_t totalAlloc;

	static uint32_t physicalMemoryBitMap[NUM_PAGE_FRAMES / 32];

	static PageDirectory *currentDirectory;
	static void initMemory(uint32_t physicalAllocStart);

	static void setBit(uint32_t frame_idx);
	static void unsetBit(uint32_t frame_idx);
	static bool testBit(uint32_t frame_idx);
	static uint32_t firstFreeFrame();
};
} // namespace cock::arch::x86

#endif // !MEMORY_HPP
