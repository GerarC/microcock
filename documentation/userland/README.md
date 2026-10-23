# Userland

Programs that run in Ring 3 under `userland/` and `test/userland/`. Only `init` is born with authority; every other program gets its capabilities from the manifest. Each program has a `README.md` next to its sources.

| Page | Topic | Program README |
|------|-------|----------------|
| [userland.md](userland.md) | Boot modules, the system call ABI, the manifest, the user library | [lib](../../userland/lib/README.md), [init](../../userland/init/README.md) |
| [init_contract.md](init_contract.md) | What the first program receives and must do, and how to replace `init` | [init](../../userland/init/README.md) |
| [keyboard.md](keyboard.md) | The PS/2 driver and how an interrupt reaches it | [kbd](../../userland/kbd/README.md) |
| [vga.md](vga.md) | The kernel console and the display server | [vga_driver](../../userland/vga_driver/README.md) |
| [console.md](console.md) | The console server: input line, serial and screen output | [console](../../userland/console/README.md) |
| [shell.md](shell.md) | The shell and its built-in commands | [sh](../../userland/sh/README.md) |
| [block_storage.md](block_storage.md) | The ATA driver, partitions, the block protocol | [ata](../../userland/ata/README.md) |
| [filesystem.md](filesystem.md) | The file protocol, `ramfs`, the `vfs` server | [ramfs](../../userland/ramfs/README.md), [vfs](../../userland/vfs/README.md) |
| [fat32.md](fat32.md) | The FAT32 server | [fat](../../userland/fat/README.md) |
| [ext2.md](ext2.md) | The ext2 server | [ext](../../userland/ext/README.md) |
| [fuzzer.md](fuzzer.md) | The Ring 3 syscall fuzzer | [test/userland](../../test/userland/README.md) |

The real time clock server has no page of its own: see [rtc](../../userland/rtc/README.md).
