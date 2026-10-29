# Boot module list

GRUB loads the boot modules (the first programs and the `initrd`) into physical memory and describes them in the Multiboot information structure. `MultibootModuleLoader` (`cock/arch/x86/loader/multiboot_module_loader.cpp`) turns that description into an array of `BootModule`, the architecture neutral type the core reads (`cock/core/boot/module.hpp`). The core never sees the Multiboot structure.

`MultibootModuleLoader::init(boot_info)` runs once, in Ring 0, during early boot:

1. It resets the count.
2. When the bootloader reported modules, it maps the module list, each module image and each name string through the [early boot window](../../core/memory/early_boot_window.md) (`BootMemory::map`).
3. A module that cannot be mapped is logged and skipped. A module with no command line gets an empty name.
4. It stops at `MAX_MODULE_NUM` (16) modules.

A null `boot_info`, or one without the modules flag, leaves the count at zero. `getModules()` and `getCount()` return the array and its length.

The pointers in the array use the boot window, so they are valid only until the window is released. `cock_main` loads the program modules and keeps the data modules before that (see [kernel main](../../core/kernel_main.md), [ELF loader](../../core/elf_loader.md) and [boot modules](../../core/memory/boot_modules.md)).
