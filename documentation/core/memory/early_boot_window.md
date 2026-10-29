# Early boot window

Before paging is complete, the Multiboot information, the memory map and the modules are read through `BootMemory` (`cock/arch/x86/boot/boot_memory.cpp`). Anything inside the first 4 MiB is reached through the kernel mapping. Anything above is mapped on demand in a fixed window at `0xF0000000` (see [address map](address_map.md)), up to 4 MiB in total. See also [bootloader](../../arch/boot/bootloader.md).

When `cock_main` has loaded the program modules it gives the whole pages of every ELF module back to the [PMM](pmm.md) (`VirtualMemoryManager::releaseBootFrames`; a partial page at either end stays reserved, and data modules such as the `initrd` are kept, see [boot modules](boot_modules.md)) and empties the window (`hal/boot.hpp`, `boot_memory_release`), after which `BootMemory::map` only serves addresses inside the first 4 MiB. The boot log has the line `Boot memory released`.
