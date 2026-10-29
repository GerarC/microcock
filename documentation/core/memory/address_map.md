# Virtual address map (x86)

| Range | Use |
|-------|-----|
| `0x00000000` to `0xBFFFFFFF` | User space. Each process has its own limit, below `0xC0000000` |
| `0x00400000` | Link address of the userland programs |
| `0x60000000` | Where anonymous `mmap` starts |
| Top 64 KiB of the user limit | User stack |
| `0xC0000000` | Kernel image (the first 4 MiB are mapped by the boot code; code and constants are read only, see [paging](paging.md#protection-of-the-kernel-image)) |
| `0xD0000000` to `0xDFFFFFFF` | Kernel heap and kernel thread stacks (see [heap](heap.md) and [vmm](vmm.md)) |
| `0xE0000000` | Temporary slots used to edit other address spaces |
| `0xF0000000` | Early boot window for Multiboot structures and modules (see [early boot window](early_boot_window.md)) |
| `0xFFC00000` and `0xFFFFF000` | Recursive page tables and page directory |

The constants live in `cock/arch/x86/memory/layout.hpp`. The core only knows the user-space limit and the start of the kernel half, both through the HAL (see [the HAL](../hal.md)).
