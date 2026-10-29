# Per-process limits

Each `Process` keeps its mappings in a **fixed table of 32 regions** (`Process::MAX_MAPPING_REGIONS`) and counts the pages it owns against a limit of **4096 pages**, 16 MiB (`Process::MAX_OWNED_PAGES`). Nothing is allocated from the kernel [heap](heap.md) for this. See [processes](../process/README.md).

* **Anonymous `mmap`** fails when the table is full, when the request would pass the page limit, or when the memory is not there. Otherwise it takes the first hole of the right size between `USER_MMAP_BASE` and the top of the image area (`Process::findFreeRange`), so an address freed by `munmap` can be handed out again.
* **The protection argument is honored.** `PROT_WRITE` makes a read write mapping; `PROT_READ` or `PROT_EXEC` alone makes a read only one (the pages are zeroed while writable and then protected, and a kernel write into them, such as `SYS_READ` into the buffer, is refused). x86 without PAE has no execute disable bit, so `PROT_EXEC` changes nothing. A protection of zero or with bits other than read, write and execute is refused, for anonymous and for `MAP_PHYS` mappings alike.
* **Device mappings** (`MAP_PHYS`) use a slot of the table but do not count as owned pages, because the frames are not RAM.
* **Shared memory mappings** use a slot of the table and do not count as owned pages; the creator of the object pays for its frames for as long as the object lives (see [shared memory](shared_memory.md)).
* **`munmap`** must name a region exactly as it was mapped; the pages go back to the [PMM](pmm.md) unless they are device pages or a shared mapping (which only drops the mapping count of the object).
* When a process dies, its whole address space is destroyed, so every owned frame returns regardless of the table.
