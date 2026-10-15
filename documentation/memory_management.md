# Memory Management Subsystem

Memory management is split into four layers, each with a single job: the Physical Memory Manager (PMM) hands out frames, `Paging` edits page tables, the Virtual Memory Manager (VMM) decides the policy, and the Kernel Heap serves small allocations.

## Address types

Physical and virtual addresses are different types, defined in `cock/core/memory/address.hpp`.

```cpp
class PhysAddr {
  public:
	constexpr explicit PhysAddr(hal::PhysicalRaw raw);
	constexpr hal::PhysicalRaw raw() const;
	constexpr PhysAddr pageBase() const;
	constexpr size_t frameNumber() const;
};

class VirtAddr {
  public:
	constexpr explicit VirtAddr(hal::VirtualRaw raw);
	constexpr hal::VirtualRaw raw() const;
	template <typename T> T *as() const;
	constexpr VirtAddr pageBase() const;
};
```

The conversion is always explicit, so a physical address cannot be passed where a virtual one is expected. The underlying integer types come from `hal/address_space.hpp` (`PhysicalRaw` is 32 bits on x86, and could be wider on another architecture). Both are plain wrappers around an integer: the compiler produces the same code as for a raw `uint32_t`.

## Virtual address map (x86)

| Range | Use |
|-------|-----|
| `0x00000000` to `0xBFFFFFFF` | User space. Each process has its own limit, below `0xC0000000` |
| `0x00400000` | Link address of the userland programs |
| `0x60000000` | Where anonymous `mmap` starts |
| Top 64 KiB of the user limit | User stack |
| `0xC0000000` | Kernel image (the first 4 MiB are mapped by the boot code) |
| `0xD0000000` to `0xDFFFFFFF` | Kernel heap and kernel thread stacks |
| `0xE0000000` | Temporary slots used to edit other address spaces |
| `0xF0000000` | Early boot window for Multiboot structures and modules |
| `0xFFC00000` and `0xFFFFF000` | Recursive page tables and page directory |

The constants live in `cock/arch/x86/memory/layout.hpp`. The core only knows the user-space limit and the start of the kernel half, both through the HAL.

## Physical Memory Manager (PMM)
The PMM manages the raw hardware RAM in 4KB chunks called "frames". It uses a bitmap that covers the whole 4 GiB range, plus a second bitmap that remembers which frames are real RAM (the rest are devices or holes).

During initialization it reads the Multiboot memory map and marks as free only the regions reported as available. It then reserves:

* the first MiB,
* the kernel image, up to `_kernel_end`,
* the Multiboot information structure, the memory map, the command lines,
* the module list and every boot module.

Allocation does not scan from zero: it remembers the last word where it found a free frame and uses `__builtin_ctz` to pick the bit.

```cpp
PhysAddr PhysicalMemoryManager::allocFrame() {
	InterruptGuard guard;

	uint32_t frame = findFreeFrame();
	if (frame == NO_FRAME) return PhysAddr();

	setFrame(frame);
	usedFrames++;
	return PhysAddr(frame * PAGE_SIZE);
}
```

`allocFrame` returns a null address when memory runs out and `freeFrame` ignores frames that are already free, so the callers decide what an out of memory condition means. `isRam` answers whether a physical address belongs to RAM; the VMM uses it to tell devices from memory.

## Paging
`Paging` (`cock/arch/x86/memory/paging.cpp`) is the only code that writes page tables. It uses the recursive mapping in the last directory entry, so every table of the current address space is reachable at a fixed virtual address.

* **Kernel half shared by all address spaces.** `Paging::init()` allocates every page table of the kernel half (directory entries 768 to 1022) at boot. From then on those entries never change, so `createAddressSpace` copies them once and a new process can never miss a kernel mapping.
* **Global pages.** When the CPU supports it, `CR4.PGE` is enabled and the kernel pages are marked `GLOBAL`, so they survive a change of `CR3`.
* **`NOT_OWNED`.** A page-table entry bit reserved for the OS. Device mappings carry it, and destroying an address space or unmapping a range does not return those frames to the PMM.
* **`destroyAddressSpace`** walks the user half of a directory, frees every owned frame and every table, and finally the directory itself.
* **`isRangeAccessible`** checks that every page in a range is present, readable by user mode when asked, and writable when asked. System calls use it before touching a user pointer.

The address space is switched only when it actually changes between two threads, and the switch is an inline write to `CR3`.

## Virtual Memory Manager (VMM)
The VMM is the policy on top of `Paging` and the PMM. It never writes a page table directly.

```cpp
class VirtualMemoryManager {
  public:
	static void *allocPages(size_t page_count, VMMPermission permission);
	static void freePages(void *vaddr, size_t page_count);
	static void *allocStack(size_t page_count);
	static void freeStack(void *base, size_t page_count);
	static bool map(PhysAddr phys, VirtAddr virt, VMMPermission permission);
	static VirtAddr mapPagesAt(VirtAddr vaddr, size_t num_pages,
							   VMMPermission permission, VirtAddr user_limit);
	static bool isUserRangeValid(VirtAddr ptr, size_t length, bool write,
								 VirtAddr user_limit);
	static bool isRangeUnmapped(VirtAddr start, size_t page_count);
	static bool isDeviceMemory(PhysAddr phys, size_t length);
};
```

* Every frame that goes to a process is **zeroed** first, so a process never sees what another left behind.
* `allocStack` leaves an unmapped **guard page** below each kernel stack, so an overflow faults instead of corrupting the heap. Freed stacks are recycled.
* `mapPagesAt` receives the user limit of the process: a request above it, or above the architecture limit, fails.
* `isUserRangeValid` is the check behind every pointer a program hands to the kernel.
* `isDeviceMemory` is true only when no frame of the range is RAM. Mapping physical memory into a process requires both that and a capability (see [capabilities](capabilities.md)).

## Kernel Heap Allocator
For dynamic memory allocation inside Ring 0 (`kmalloc` and `kfree`, behind `new` and `delete`), the OS uses a custom Heap implementation based on a First-Fit linked list algorithm.
* **Alignment:** blocks are aligned and their header is 16 bytes, so the pointer handed out is aligned on every architecture.
* **Splitting:** If a requested allocation is smaller than a free block, the allocator splits the block to reduce internal fragmentation.
* **Coalescing:** When memory is freed, the allocator merges adjacent free blocks to prevent external fragmentation.
* **Interrupt safety:** each operation runs under an `InterruptGuard`, which saves the interrupt flag, disables interrupts and restores the previous state, so guards nest.
* **Growth:** when no block fits, the heap asks the VMM for more pages. It does not return them.

```cpp
void *Heap::allocate(size_t size) {
	if (size == 0) return nullptr;
	if (size > SIZE_MAX - sizeof(MemoryBlock) - ALIGNMENT - hal::PAGE_SIZE)
		return nullptr;

	hal::InterruptGuard guard;
	size_t aligned_size = alignUp(size);
	MemoryBlock *prev = nullptr;
	MemoryBlock *block = findFirstAvailable(aligned_size, &prev);
	...
}
```

`kcalloc` checks for overflow in `num * size`. `operator new` stops the kernel with a panic when the heap cannot grow.

## Early boot window
Before paging is complete, the Multiboot information, the memory map and the modules are read through `BootMemory` (`cock/arch/x86/boot/boot_memory.cpp`). Anything inside the first 4 MiB is reached through the kernel mapping. Anything above is mapped on demand in a fixed window at `0xF0000000`, up to 4 MiB in total.
