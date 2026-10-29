# Data boot modules

A Multiboot module that does not start with the ELF magic is not loaded as a program: `cock_main` keeps it as a **fixed** [shared memory](shared_memory.md) object (`SharedMemory::createFixed`), named after its command line and tagged with its index in the module list. The frames are the ones GRUB placed the module in, already reserved by the [PMM](pmm.md), so the object charges nobody's quota and is never freed, whatever its holders do. A module of more than `MAX_OBJECT_PAGES` pages, with a name of 32 bytes or more, a duplicate name, or a start that is not page aligned, is refused with an error in the log. The last page may carry bytes after the end of the module.

* `SYS_MODULE_OPEN(name, length)` validates the user string and looks the module up. The caller needs a `BOOT_MODULE` capability that covers the module index (see [capabilities](../capabilities.md)); it then becomes a **read only** holder and gets the object id, which it maps with `SYS_SHM_MAP`. Nobody can hold a fixed object with write access, so a module can be read and shared read only but never changed.
* An unknown name and a missing capability both return `-1`.
* The root server holds `BOOT_MODULE` for every module with the right to grant.

The modules that are programs are loaded at boot by the kernel (see [kernel main](../kernel_main.md)); their frames are then returned (see [early boot window](early_boot_window.md)).
