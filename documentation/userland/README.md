# Userland

Programs that run in Ring 3 under `userland/` and `test/userland/`. Only `rs` is born with authority; every other program gets its capabilities from the manifest. Each program has a `README.md` next to its sources. The folders here follow the ones of the source tree.

Everything that is not the kernel runs in Ring 3 as a program loaded from a Multiboot module, or from a file of the initrd.

## system/

How the programs are described, built and started.

| Page | Topic |
|------|-------|
| [programs.md](system/programs.md) | Every program that ships with the system, and the test programs |
| [boot.md](system/boot.md) | How the system starts the userland, from the modules to a key press |
| [manifest.md](system/manifest.md) | `manifest.conf`: which programs run and what authority each one gets |
| [init_contract.md](system/init_contract.md) | What the first program receives and must do, and how to replace `rs` |
| [building.md](system/building.md) | How to write and build a program |

## drivers/

| Page | Topic | Program README |
|------|-------|----------------|
| [keyboard.md](drivers/keyboard.md) | The PS/2 driver and how an interrupt reaches it | [kbd](../../userland/drivers/kbd/README.md) |
| [vga.md](drivers/vga.md) | The kernel console and the display server | [vga_driver](../../userland/drivers/vga_driver/README.md) |
| [console.md](drivers/console.md) | The console driver: serial and screen output | [console](../../userland/drivers/console/README.md) |
| [block_storage.md](drivers/block_storage.md) | The ATA driver, partitions, the block protocol | [ata](../../userland/drivers/ata/README.md) |

The real time clock server has no page of its own: see [rtc](../../userland/drivers/rtc/README.md).

## servers/

| Page | Topic | Program README |
|------|-------|----------------|
| [pm.md](servers/pm.md) | The process manager, `rs` and `init`: who creates processes and who keeps the table | [pm](../../userland/servers/pm/README.md), [rs](../../userland/servers/rs/README.md), [init](../../userland/servers/init/README.md) |
| [names.md](servers/names.md) | The name server: connecting to a server by name, with the `ipc=` policy checked then | [names](../../userland/servers/names/README.md) |
| [pipe.md](servers/pipe.md) | The pipe server: pipes between processes, with readers and writers held while a pipe is empty or full | [pipe](../../userland/servers/pipe/README.md) |
| [tty.md](servers/tty.md) | The tty server: the typed line and the readers | [tty](../../userland/servers/tty/README.md) |

### servers/fs/

| Page | Topic | Program README |
|------|-------|----------------|
| [filesystem.md](servers/fs/filesystem.md) | The file protocol, `ramfs`, the `vfs` server | [ramfs](../../userland/servers/ramfs/README.md), [vfs](../../userland/servers/vfs/README.md) |
| [fat32.md](servers/fs/fat32.md) | The FAT32 server | [fat](../../userland/servers/fat/README.md) |
| [ext2.md](servers/fs/ext2.md) | The ext2 server | [ext](../../userland/servers/ext/README.md) |

## apps/

| Page | Topic | Program README |
|------|-------|----------------|
| [shell.md](apps/shell.md) | The shell and its built-in commands | [sh](../../userland/apps/sh/README.md) |

## testing/

| Page | Topic | Program README |
|------|-------|----------------|
| [README.md](testing/README.md) | The end to end QEMU test and the host tests | [test/userland](../../test/userland/README.md) |
| [fuzzer.md](testing/fuzzer.md) | The Ring 3 syscall fuzzer | [test/userland](../../test/userland/README.md) |

The system call interface that every program uses is in [libs](../libs/README.md): [the `cock::user` API](../libs/user_api.md) and [the system call ABI](../libs/syscall_abi.md).
