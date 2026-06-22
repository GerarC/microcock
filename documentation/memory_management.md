# Memory Management Subsystem

The memory management in MicroCOCK is divided into three distinct layers: the Physical Memory Manager (PMM), the Virtual Memory Manager (VMM), and the Kernel Heap Allocator.

## Physical Memory Manager (PMM)
The PMM manages the raw hardware RAM in 4KB chunks called "frames". It utilizes a highly efficient bitmap array to track whether a frame is free or in use. During initialization, it reads the Multiboot memory map to determine available RAM and protect reserved areas.

When the system requests a frame, the PMM scans the bitmap for the first available bit.

```cpp
uint32_t PhysicalMemoryManager::allocFrame() {
    uint32_t frame = firstFreeFrame();
    if (frame == LAST_ADDRESS) { panic("PH OUT OF MEMORY"); } 
    setBit(frame);
    totalAlloc++;

    return frame * PAGE_SIZE; 
}
```

## Virtual Memory Manager (VMM)
The VMM abstracts the x86 hardware paging structures (Page Directories and Page Tables). It is responsible for mapping the physical frames provided by the PMM to contiguous virtual addresses, enforcing memory protection boundaries (Ring 0 vs Ring 3).

When creating a new Userland process, the VMM creates a completely new, isolated Page Directory (Address Space) that inherits the Kernel's higher-half mappings 

```cpp
void VirtualMemoryManager::map(uintptr_t phys, uintptr_t virt, VMMPermission permission) {
    PhysicalMemoryManager::mapPage(phys, virt, toX86Flag(permission)); 
}
```

## Kernel Heap Allocator
For dynamic memory allocation inside Ring 0 (`kmalloc` and `kfree`), the OS uses a custom Heap implementation based on a First-Fit linked list algorithm. 
* **Splitting:** If a requested allocation is smaller than a free block, the allocator splits the block to reduce internal fragmentation.
* **Coalescing:** When memory is freed, the allocator merges adjacent free blocks to prevent external fragmentation.

```cpp
void* Heap::allocate(size_t size) {
    if (size == 0) return nullptr;
    size_t aligned_size = alignUp(size);
    MemoryBlock* prev = nullptr;
    MemoryBlock* block = findFirstAvailable(aligned_size, &prev); 
    
    // ... handles requesting new pages if out of heap space ...
    
    split(block, aligned_size);
    removeFreeBlock(block, prev);
    return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(block) + sizeof(MemoryBlock)); 
}
```
