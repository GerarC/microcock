# Userland

Programs that run in Ring 3 under `userland/` and `test/userland/`. Only `rs` is born with authority; every other program gets its capabilities from the manifest. Each program has a `README.md` next to its sources.

| Page | Topic | Program README |
|------|-------|----------------|
| [userland.md](userland.md) | Boot modules, the system call ABI, the manifest, the user library | [lib](../../userland/lib/README.md), [rs](../../userland/servers/rs/README.md) |
| [init_contract.md](init_contract.md) | What the first program receives and must do, and how to replace `rs` | [rs](../../userland/servers/rs/README.md) |
| [pm.md](pm.md) | The process manager, `rs` and `init`: who creates processes and who keeps the table | [pm](../../userland/servers/pm/README.md), [rs](../../userland/servers/rs/README.md), [init](../../userland/servers/init/README.md) |
| [names.md](names.md) | The name server: connecting to a server by name, with the `ipc=` policy checked then | [names](../../userland/servers/names/README.md) |
| [pipe.md](pipe.md) | The pipe server: pipes between processes, with readers and writers held while a pipe is empty or full | [pipe](../../userland/servers/pipe/README.md) |
| [keyboard.md](keyboard.md) | The PS/2 driver and how an interrupt reaches it | [kbd](../../userland/drivers/kbd/README.md) |
| [vga.md](vga.md) | The kernel console and the display server | [vga_driver](../../userland/drivers/vga_driver/README.md) |
| [console.md](console.md) | The console driver: serial and screen output | [console](../../userland/drivers/console/README.md) |
| [tty.md](tty.md) | The tty server: the typed line and the readers | [tty](../../userland/servers/tty/README.md) |
| [shell.md](shell.md) | The shell and its built-in commands | [sh](../../userland/apps/sh/README.md) |
| [block_storage.md](block_storage.md) | The ATA driver, partitions, the block protocol | [ata](../../userland/drivers/ata/README.md) |
| [filesystem.md](filesystem.md) | The file protocol, `ramfs`, the `vfs` server | [ramfs](../../userland/servers/ramfs/README.md), [vfs](../../userland/servers/vfs/README.md) |
| [fat32.md](fat32.md) | The FAT32 server | [fat](../../userland/servers/fat/README.md) |
| [ext2.md](ext2.md) | The ext2 server | [ext](../../userland/servers/ext/README.md) |
| [fuzzer.md](fuzzer.md) | The Ring 3 syscall fuzzer | [test/userland](../../test/userland/README.md) |

The real time clock server has no page of its own: see [rtc](../../userland/drivers/rtc/README.md).
