#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stdint.h>
#include <string.h>

extern "C" void flush_tlb();
extern "C" void invalpg(void *addr);

namespace cock::arch::x86 {
using cock::utils::Logger;
using cock::utils::panic;

constexpr uint32_t PAGE_SIZE = 0x1000;
extern "C" uint32_t page_directory[PAGE_DIR_SIZE];

// Initialization
MBInfo *PhysicalMemoryManager::bootInfo = nullptr;
uint32_t PhysicalMemoryManager::pageFrameMin;
uint32_t PhysicalMemoryManager::pageFrameMax;
uint32_t PhysicalMemoryManager::totalAlloc;
uint32_t PhysicalMemoryManager::physicalMemoryBitMap[NUM_PAGE_FRAMES / 32];
PageDirectory *PhysicalMemoryManager::currentDirectory = nullptr;

void PhysicalMemoryManager::init(MBInfo *boot_info) {
	bootInfo = boot_info;
	uint32_t physical_alloc_start = 0;

	if ((bootInfo->flags & (1 << 3)) && bootInfo->mods_count > 0) {

		uint32_t mods_virt = bootInfo->mods_address + KERNEL_START;

		uint32_t mod1 = *reinterpret_cast<uint32_t *>(mods_virt + 4);
		physical_alloc_start = (mod1 + TABLE_MASK) & ~TABLE_MASK;

	} else {
		physical_alloc_start = 0x400000;
	}

	initMemory(physical_alloc_start);
	Logger::trace("Memory Manager initializated");
}

void PhysicalMemoryManager::initMemory(uint32_t physicalAllocStart) {
	uint32_t mem_high = bootInfo->mem_upper * PAGE_DIR_SIZE;

	currentDirectory = reinterpret_cast<PageDirectory *>(&page_directory);

	pmmInit(physicalAllocStart, mem_high);
}

void PhysicalMemoryManager::invalidatePage(uint32_t virtual_address) {
	asm volatile("invlpg (%0)" ::"r"(virtual_address) : "memory");
}

void PhysicalMemoryManager::pmmInit(uint32_t mem_low, uint32_t mem_high) {
	pageFrameMin = utils::ceil_div(mem_low, PAGE_SIZE);
	pageFrameMax = mem_high / PAGE_SIZE;
	totalAlloc = 0;

	memset(physicalMemoryBitMap, 0, sizeof(physicalMemoryBitMap));
    for(uint32_t frame_idx = 0; frame_idx < pageFrameMin; frame_idx++) setBit(frame_idx);
}

void PhysicalMemoryManager::setBit(uint32_t frame_idx) {
	physicalMemoryBitMap[frame_idx / FRAME_SIZE] |=
		(1 << (frame_idx % FRAME_SIZE));
}

void PhysicalMemoryManager::unsetBit(uint32_t frame_idx) {
	physicalMemoryBitMap[frame_idx / FRAME_SIZE] &=
		~(1 << (frame_idx % FRAME_SIZE));
}

bool PhysicalMemoryManager::testBit(uint32_t frame_idx) {
	return physicalMemoryBitMap[frame_idx / FRAME_SIZE] &
		   (1 << (frame_idx % FRAME_SIZE));
}

uint32_t PhysicalMemoryManager::firstFreeFrame() {
	for (uint32_t page_frame_idx = 0;
		 page_frame_idx < (NUM_PAGE_FRAMES / FRAME_SIZE); page_frame_idx++) {
		if (physicalMemoryBitMap[page_frame_idx] != MAX_PAGE_ADDRESS)
			for (uint32_t offset = 0; offset < FRAME_SIZE; offset++) {
				uint32_t bit = 1 << offset;
				if (!(physicalMemoryBitMap[page_frame_idx] & bit))
					return page_frame_idx * FRAME_SIZE + offset;
			}
	}
	return MAX_PAGE_ADDRESS; //Out of Memory
}

uint32_t PhysicalMemoryManager::allocFrame(){
    uint32_t frame = firstFreeFrame();
    if(frame == MAX_PAGE_ADDRESS){
        panic("PMM OUT OF MEMORY");
    }
    setBit(frame);
    totalAlloc++;

    return frame * PAGE_SIZE;
}

void PhysicalMemoryManager::freeFrame(uint32_t phys_addr){
    uint32_t frame = phys_addr / PAGE_SIZE;
    unsetBit(frame);
    totalAlloc--;
}

void PhysicalMemoryManager::mapPage(uint32_t phys, uint32_t virt, uint32_t flags){
    uint32_t pd_idx = virt >> PAGE_DIRECTORY_IDX_OFFSET;
    uint32_t pt_idx = (virt >> PAGE_TABLE_IDX_OFFSET) & DIRECTORY_MASK;

    PageDirectory* virtual_pd = reinterpret_cast<PageDirectory*>(MAX_PAGE_ADDRESS-TABLE_MASK);

    if(!(virtual_pd->entries[pd_idx] & PRESENT)){
        uint32_t pt_phys = allocFrame();
        virtual_pd->entries[pd_idx] = pt_phys | PRESENT | WRITEABLE | (flags & USER);

        uint32_t pt_virt = 0xFFC00000 + (pd_idx*PAGE_SIZE);


        invalidatePage(pt_virt);

        memset(reinterpret_cast<void*>(pt_virt), 0, PAGE_SIZE);
    }

    uint32_t pt_virt = 0xFFC00000 + (pd_idx*PAGE_SIZE);
    PageTable* table = reinterpret_cast<PageTable*>(pt_virt);
    table->entries[pt_idx] = (phys & ~TABLE_MASK) | flags;
    invalidatePage(virt);
}

void PhysicalMemoryManager::unmapPage(uint32_t virt){
    uint32_t pd_idx = virt >> PAGE_DIRECTORY_IDX_OFFSET;
    uint32_t pt_idx = (virt >> PAGE_TABLE_IDX_OFFSET) & DIRECTORY_MASK;
    PageDirectory* virtual_pd = reinterpret_cast<PageDirectory*>(MAX_PAGE_ADDRESS-TABLE_MASK);

    if(!(virtual_pd->entries[pd_idx] & PRESENT)) return;

    uint32_t pt_virt = 0xFFC00000 + (pd_idx*PAGE_SIZE);
    PageTable* table = reinterpret_cast<PageTable*>(pt_virt);
    table->entries[pt_idx] = 0;

    invalidatePage(virt);
}

uint32_t PhysicalMemoryManager::getPhysicalAddress(uint32_t virt){
    uint32_t pd_idx = virt >> PAGE_DIRECTORY_IDX_OFFSET;
    uint32_t pt_idx = (virt >> PAGE_TABLE_IDX_OFFSET) & DIRECTORY_MASK;
    PageDirectory* virtual_pd = reinterpret_cast<PageDirectory*>(MAX_PAGE_ADDRESS-TABLE_MASK);

    if(!(virtual_pd->entries[pd_idx] & PRESENT)) return NULL_PTR;

    uint32_t pt_virt = 0xFFC00000 + (pd_idx*PAGE_SIZE);
    PageTable* table = reinterpret_cast<PageTable*>(pt_virt);
    if(!(table->entries[pt_idx] & PRESENT)) return NULL_PTR;

    return (table->entries[pt_idx] & ~TABLE_MASK) + (virt & TABLE_MASK);

}


} // namespace cock::arch::x86PAGE_DIRECTORY_IDX_OFFSET
