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
| `0xC0000000` | Kernel image (the first 4 MiB are mapped by the boot code; code and constants are read only) |
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
	freeFrames--;
	return PhysAddr(frame * PAGE_SIZE);
}
```

`allocFrame` returns a null address when memory runs out and `freeFrame` only releases frames that `allocFrame` handed out (a separate bitmap records them), so a double free, a reserved frame of the kernel or of the boot structures, or an address outside RAM is ignored, and the callers decide what an out of memory condition means. `isRam` answers whether a physical address belongs to RAM; the VMM uses it to tell devices from memory. `freeFrameCount` returns the number of free frames, kept as a counter, so a caller can check before starting a large mapping.

## Paging
`Paging` (`cock/arch/x86/memory/paging.cpp`) is the only code that writes page tables. It uses the recursive mapping in the last directory entry, so every table of the current address space is reachable at a fixed virtual address.

* **Kernel half shared by all address spaces.** `Paging::init()` allocates every page table of the kernel half (directory entries 768 to 1022) at boot. From then on those entries never change, so `createAddressSpace` copies them once and a new process can never miss a kernel mapping.
* **Global pages.** When the CPU supports it, `CR4.PGE` is enabled and the kernel pages are marked `GLOBAL`, so they survive a change of `CR3`.
* **`NOT_OWNED`.** A page-table entry bit reserved for the OS. Device mappings carry it, and destroying an address space or unmapping a range does not return those frames to the PMM.
* **`destroyAddressSpace`** walks the user half of a directory, frees every owned frame and every table, and finally the directory itself.
* **`isRangeAccessible`** checks that every page in a range is present, readable by user mode when asked, and writable when asked. System calls use it before touching a user pointer.

The address space is switched only when it actually changes between two threads, and the switch is an inline write to `CR3`.

### Protection of the kernel image
The boot code maps the first 4 MiB writable. At the end of `Paging::init()`, `protectKernelImage` clears the write bit of every page of `.text` and of `.rodata` (the linker script exports `_text_start`, `_text_end`, `_rodata_start` and `_rodata_end`) and then turns on `CR0.WP`, so the kernel itself obeys the read-only bit. A stray write through a bad pointer faults instead of silently changing code or constants. The linker also checks, with an `ASSERT`, that the image fits in the 4 MiB that the boot code maps.

Because the kernel obeys the write bit of user pages as well, nothing in the kernel writes to a user page that is read only. The ELF loader maps every segment writable, copies it, and only then makes the read-only segments read only; a page shared with a writable segment stays writable.

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
	static bool protectPage(VirtAddr virt, VMMPermission permission);
	static bool isRangeUnmapped(VirtAddr start, size_t page_count);
	static bool isDeviceMemory(PhysAddr phys, size_t length);
};
```

* Every frame that goes to a process is **zeroed** first, so a process never sees what another left behind.
* `allocStack` leaves an unmapped **guard page** below each kernel stack, so an overflow faults instead of corrupting the heap.
* The kernel virtual range (`0xD0000000` to `0xE0000000`) is managed by a `RangePool` (`shared/include/cock/data_structure/range_pool.hpp`): a bump cursor plus a sorted table of up to 128 free ranges. `allocPages` and `allocStack` take the first free range that fits (or move the cursor), `freePages` and `freeStack` give the range back (a stack with its guard page), neighbours are merged, and a range that touches the cursor moves it back. When the table is full a release that cannot be merged is dropped, so that range is lost. A `freePages` of an address outside the range (a user mapping) only unmaps.
* `mapPagesAt` receives the user limit of the process: a request above it, or above the architecture limit, fails. Before mapping anything it counts the frames the request needs (the new pages, plus a margin for page tables) and fails at once if the PMM does not have them. For a user mapping it also leaves `KERNEL_RESERVED_FRAMES` frames untouched, so a program cannot take the memory the kernel needs for stacks, page tables and its heap. If a mapping of fresh pages fails half way, the pages it had just mapped are released.
* `protectPage` replaces the permissions of one already mapped page.
* `isUserRangeValid` is the check behind every pointer a program hands to the kernel.
* `isDeviceMemory` is true only when no frame of the range is RAM. Mapping physical memory into a process requires both that and a capability (see [capabilities](capabilities.md)).

## Per-process limits
Each `Process` keeps its mappings in a **fixed table of 32 regions** (`Process::MAX_MAPPING_REGIONS`) and counts the pages it owns against a limit of **4096 pages**, 16 MiB (`Process::MAX_OWNED_PAGES`). Nothing is allocated from the kernel heap for this.

* **Anonymous `mmap`** fails when the table is full, when the request would pass the page limit, or when the memory is not there. Otherwise it takes the first hole of the right size between `USER_MMAP_BASE` and the top of the image area (`Process::findFreeRange`), so an address freed by `munmap` can be handed out again.
* **The protection argument is honored.** `PROT_WRITE` makes a read write mapping; `PROT_READ` or `PROT_EXEC` alone makes a read only one (the pages are zeroed while writable and then protected, and a kernel write into them, such as `SYS_READ` into the buffer, is refused). x86 without PAE has no execute disable bit, so `PROT_EXEC` changes nothing. A protection of zero or with bits other than read, write and execute is refused, for anonymous and for `MAP_PHYS` mappings alike.
* **Device mappings** (`MAP_PHYS`) use a slot of the table but do not count as owned pages, because the frames are not RAM.
* **Shared memory mappings** use a slot of the table and do not count as owned pages; the creator of the object pays for its frames for as long as the object lives.
* **`munmap`** must name a region exactly as it was mapped; the pages go back to the PMM unless they are device pages or a shared mapping (which only drops the mapping count of the object).
* When a process dies, its whole address space is destroyed, so every owned frame returns regardless of the table.

## Shared memory
A **shared memory object** is a list of frames kept by the kernel (`SharedMemory`, `cock/core/memory/shared_memory.*`). The table is fixed: 32 objects (`MAX_OBJECTS`) of at most 256 pages (1 MiB) each (`MAX_OBJECT_PAGES`), with at most 8 holders per object (`MAX_HOLDERS`). Nothing comes from the kernel heap. The `initrd` is one such object, so the archive cannot be larger than 1 MiB: with 128 pages the test initrd (every test program, 530 KiB) no longer fit and the kernel logged `Cannot keep data module initrd`.

* `SYS_SHM_CREATE(pages)` takes zeroed frames, all or none and leaving the kernel reserve free (`VirtualMemoryManager::allocFrames`), charges the pages to the creator against `Process::MAX_OWNED_PAGES`, and returns the object id. The creator is the first holder, with write access.
* `SYS_SHM_SHARE(id, pid, writable)` makes a live process a holder. Only a holder can share, nobody can give more access than it has, and the two processes need an IPC capability between them (see [capabilities](capabilities.md#the-ipc-capability)). Sharing again with a holder only raises its access.
* `SYS_SHM_MAP(id, writable)` maps the whole object at a free range, found as for anonymous `mmap`, and returns the address. The caller must hold the object, and a writable mapping needs write access. The pages are mapped with the `SHARED` permission, which marks them as not owned, so `munmap` and the death of the process never return the frames; a read only mapping has no write bit. The mapping uses a slot of the region table, records the object id, and does not count as owned pages.
* `SYS_SHM_RELEASE(id)` drops the right of the caller. Mappings it already made stay until they are unmapped.
* An object is freed when it has no holders and no mappings left: its frames go back to the PMM and the pages are returned to the quota of the creator if it is still alive. When a process dies the kernel drops its mappings and its rights (`SharedMemory::dropProcess`), and an object whose creator died stays until the last user lets go.

### Data boot modules
A Multiboot module that does not start with the ELF magic is not loaded as a program: `cock_main` keeps it as a **fixed** shared memory object (`SharedMemory::createFixed`), named after its command line and tagged with its index in the module list. The frames are the ones GRUB placed the module in, already reserved by the PMM, so the object charges nobody's quota and is never freed, whatever its holders do. A module of more than `MAX_OBJECT_PAGES` pages, with a name of 32 bytes or more, a duplicate name, or a start that is not page aligned, is refused with an error in the log. The last page may carry bytes after the end of the module.

* `SYS_MODULE_OPEN(name, length)` validates the user string and looks the module up. The caller needs a `BOOT_MODULE` capability that covers the module index; it then becomes a **read only** holder and gets the object id, which it maps with `SYS_SHM_MAP`. Nobody can hold a fixed object with write access, so a module can be read and shared read only but never changed.
* An unknown name and a missing capability both return `-1`.
* The root server holds `BOOT_MODULE` for every module with the right to grant.

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

When `cock_main` has loaded the program modules it gives the whole pages of every ELF module back to the PMM (`VirtualMemoryManager::releaseBootFrames`; a partial page at either end stays reserved, and data modules such as the `initrd` are kept) and empties the window (`hal/boot.hpp`, `boot_memory_release`), after which `BootMemory::map` only serves addresses inside the first 4 MiB. The boot log has the line `Boot memory released`.
