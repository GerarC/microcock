# Memory Management Subsystem

Memory management is split into four layers, each with a single job: the Physical Memory Manager ([PMM](pmm.md)) hands out frames, [paging](paging.md) edits page tables, the Virtual Memory Manager ([VMM](vmm.md)) decides the policy, and the kernel [heap](heap.md) serves small allocations.

| Page | Concept |
|------|---------|
| [address_types.md](address_types.md) | `PhysAddr` and `VirtAddr` |
| [address_map.md](address_map.md) | The virtual address map of x86 |
| [pmm.md](pmm.md) | Frames, the bitmaps and the reference counts |
| [paging.md](paging.md) | Page tables, the kernel half, protection of the kernel image |
| [copy_on_write.md](copy_on_write.md) | Sharing frames between address spaces after `fork` |
| [vmm.md](vmm.md) | The policy on top of paging and the PMM |
| [process_limits.md](process_limits.md) | Region table and page quota of each process, `mmap` rules |
| [shared_memory.md](shared_memory.md) | Shared memory objects and `SYS_SHM_*` |
| [boot_modules.md](boot_modules.md) | Boot modules that are data, not programs |
| [heap.md](heap.md) | The kernel heap allocator |
| [early_boot_window.md](early_boot_window.md) | Reading bootloader data before paging is complete |
