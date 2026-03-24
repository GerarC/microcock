#ifndef MEMORY_HPP
#define MEMORY_HPP
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

class PhysicalMemoryManager {
  public:
	static void init(MBInfo *boot_info);
	static void pmmInit(uint32_t mem_low, uint32_t mem_high);
	static void invalidatePage(uint32_t virtual_address);

	static uint32_t allocFrame();
	static void freeFrame(uint32_t phys_addr);

	static void mapPage(uint32_t phys, uint32_t virt, uint32_t flags);
	static void unmapPage(uint32_t virt);
    static uint32_t getPhysicalAddress(uint32_t virt);

  private:
	static constexpr uint32_t PAGE_SIZE = 0x1000;
	static constexpr uint32_t NUM_PAGE_DIRS = 0x100;
	static constexpr uint32_t NUM_PAGE_FRAMES = (0x100000000ULL / PAGE_SIZE / 8);

	static constexpr uintptr_t KERNEL_START = 0xC0000000;

	static MBInfo *bootInfo;
	static uint32_t pageFrameMin;
	static uint32_t pageFrameMax;
	static uint32_t totalAlloc;

	static uint32_t physicalMemoryBitMap[NUM_PAGE_FRAMES / 32];

    static PageDirectory* currentDirectory;

	static void initMemory(uint32_t physicalAllocStart);

	static void setBit(uint32_t frame_idx);
	static void unsetBit(uint32_t frame_idx);
	static bool testBit(uint32_t frame_idx);
    static uint32_t firstFreeFrame();
};

} // namespace cock::arch::x86

#endif // !MEMORY_HPP
