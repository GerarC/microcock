# Virtual Memory Manager (VMM)

The VMM is the policy on top of [paging](paging.md) and the [PMM](pmm.md). It never writes a page table directly.

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
	static PhysAddr cloneAddressSpace(PhysAddr source);
	static bool resolveCopyOnWrite(VirtAddr virt);
	static bool isRangeUnmapped(VirtAddr start, size_t page_count);
	static bool isDeviceMemory(PhysAddr phys, size_t length);
};
```

* Every frame that goes to a process is **zeroed** first, so a process never sees what another left behind.
* `allocStack` leaves an unmapped **guard page** below each kernel stack, so an overflow faults instead of corrupting the heap.
* The kernel virtual range (`0xD0000000` to `0xE0000000`) is managed by a `RangePool` (`shared/include/cock/data_structure/range_pool.hpp`): a bump cursor plus a sorted table of up to 128 free ranges. `allocPages` and `allocStack` take the first free range that fits (or move the cursor), `freePages` and `freeStack` give the range back (a stack with its guard page), neighbours are merged, and a range that touches the cursor moves it back. When the table is full a release that cannot be merged is dropped, so that range is lost. A `freePages` of an address outside the range (a user mapping) only unmaps.
* `mapPagesAt` receives the user limit of the process: a request above it, or above the architecture limit, fails. Before mapping anything it counts the frames the request needs (the new pages, plus a margin for page tables) and fails at once if the PMM does not have them. For a user mapping it also leaves `KERNEL_RESERVED_FRAMES` frames untouched, so a program cannot take the memory the kernel needs for stacks, page tables and its heap. If a mapping of fresh pages fails half way, the pages it had just mapped are released.
* `protectPage` replaces the permissions of one already mapped page.
* `isUserRangeValid` is the check behind every pointer a program hands to the kernel. For a range the kernel is going to write it also gives private frames to the copy on write pages in it.
* `cloneAddressSpace` and `resolveCopyOnWrite` are the two halves of [copy on write](copy_on_write.md).
* `isDeviceMemory` is true only when no frame of the range is RAM. Mapping physical memory into a process requires both that and a capability (see [capabilities](../capabilities.md)).
* `releaseBootFrames` gives the pages of the boot modules back to the PMM once they are loaded (see [early boot window](early_boot_window.md)).
