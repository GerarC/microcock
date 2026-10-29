# Paging

`Paging` (`cock/arch/x86/memory/paging.cpp`) is the only code that writes page tables. It uses the recursive mapping in the last directory entry, so every table of the current address space is reachable at a fixed virtual address.

* **Kernel half shared by all address spaces.** `Paging::init()` allocates every page table of the kernel half (directory entries 768 to 1022) at boot. From then on those entries never change, so `createAddressSpace` copies them once and a new process can never miss a kernel mapping.
* **Global pages.** When the CPU supports it, `CR4.PGE` is enabled and the kernel pages are marked `GLOBAL`, so they survive a change of `CR3`.
* **`NOT_OWNED`.** A page-table entry bit reserved for the OS. Device mappings carry it, and destroying an address space or unmapping a range does not return those frames to the [PMM](pmm.md).
* **`COPY_ON_WRITE`.** Another page-table entry bit reserved for the OS (bit 10): the page was writable and its frame is shared with another address space, so the entry is read only until the first write. See [copy on write](copy_on_write.md).
* **`destroyAddressSpace`** walks the user half of a directory, drops the reference of every owned frame (the frame is freed with its last reference) and frees every table, and finally the directory itself.
* **`isRangeAccessible`** checks that every page in a range is present, readable by user mode when asked, and writable when asked. System calls use it before touching a user pointer. A user range checked for writing may hold copy on write pages: each one gets its own frame first, because the kernel writes with `CR0.WP` set and would fault on a read only page.

The address space is switched only when it actually changes between two threads, and the switch is an inline write to `CR3` (see [switching](../scheduler/switching.md)).

## Protection of the kernel image

The boot code maps the first 4 MiB writable. At the end of `Paging::init()`, `protectKernelImage` clears the write bit of every page of `.text` and of `.rodata` (the linker script exports `_text_start`, `_text_end`, `_rodata_start` and `_rodata_end`) and then turns on `CR0.WP`, so the kernel itself obeys the read-only bit. A stray write through a bad pointer faults instead of silently changing code or constants. The linker also checks, with an `ASSERT`, that the image fits in the 4 MiB that the boot code maps.

Because the kernel obeys the write bit of user pages as well, nothing in the kernel writes to a user page that is read only. The ELF loader maps every segment writable, copies it, and only then makes the read-only segments read only; a page shared with a writable segment stays writable.
