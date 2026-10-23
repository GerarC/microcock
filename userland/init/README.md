# init

The root server of MicroCOCK. It is the only process the kernel starts with authority: it holds the root capabilities and the right to grant them. Everything else in Ring 3 is started by `init`, according to a text file, and receives only what that file says.

## Where it sits

- It is a Multiboot module named `init` (see `USERLAND_BOOT_ORDER` in the root `Makefile`: `init`, `ramfs`, `vga_driver`). The kernel gives the root capabilities to the module with that name.
- It needs no line in the manifest: it is the one that reads it. Its own policy is written in the code: grant `ramfs` the `initrd` boot module and tell it so.
- It runs once, starts the system and exits with status 0. It exits with status 1 if `ramfs` or the `initrd` module is missing, if `manifest.conf` cannot be read, or if the parser rejects a line.
- The C library console client is switched off (`CONSOLE_CLIENT = no`): `init` prints before `console` has its capabilities.

## Protocol

| Direction | Message | Use |
|-----------|---------|-----|
| `init` to `ramfs` | `CAPABILITY_GRANTED` (after `SYS_CAP_GRANT` of `BOOT_MODULE`) | Lets `ramfs` open the `initrd` |
| `init` to `ramfs` | `FS_CONNECT`, `FS_OPEN`, `FS_STAT`, `FS_READ`, `FS_CLOSE` | Reads `manifest.conf` and each program image, through `cock::user::fs::Client` |
| `init` to each started process | `CAPABILITY_GRANTED`, `arg0` is the process id of the `peer`, `arg1` is the `arg=` value | Sent after the grants, only when the entry has grants, a peer or an argument |
| kernel to `init` | `PROCESS_EXITED` | Only for `repeat=` entries, which `init` spawns with the exit notice requested and supervises |

The manifest grammar (`shared/include/cock/abi/manifest.hpp`) has these keys after the entry name:

| Key | Meaning |
|-----|---------|
| `path=` | File of the initrd to spawn; without it the entry is a boot module that is only configured |
| `caps=` | Up to four grants: `io:first+count`, `irq:line`, `mem:first+size` |
| `peer=` | Name of a process whose id travels in `CAPABILITY_GRANTED` |
| `arg=` | One number that travels in `CAPABILITY_GRANTED` (the block device of `fat` and `ext`) |
| `repeat=` | Run the program that many times in a row, 1 to 32 |

## What the manifests grant

| Entry | Source | Grants and arguments |
|-------|--------|----------------------|
| `vga_driver` | `initrd/manifest.conf` | `mem:0xA0000+0x20000`, boot module |
| `console` | `initrd/manifest.conf` | `io:0x3F8+8`, peer `vga_driver` |
| `ata` | `initrd/manifest.conf` | `io:0x1F0+8`, `io:0x3F6+1` |
| `rtc` | `initrd/manifest.conf` | `io:0x70+2` |
| `fat`, `ext` | `initrd/manifest.conf` | `arg=2`, `arg=3`, no capabilities |
| `vfs`, `sh` | `initrd/manifest.conf` | none |
| `kbd` | `initrd/manifest.conf` | `io:0x60+1`, `irq:1`, peer `console` |
| `fsclient`, `probe`, `ext4probe` and the other test programs | `test/initrd/manifest.conf` | none (`ext4probe` is `ext.elf` with `arg=4`); only with `TEST_INITRD=1` |

The kernel still checks every grant against what `init` holds, so a manifest line cannot give more than `init` has.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `grantInitrd`, `loadManifest`, `runEntry` for each entry, exit |
| `include/init/boot.hpp`, `src/boot.cpp` | `grantInitrd()`: the policy in the code, the `initrd` boot module for `ramfs` |
| `include/init/grant.hpp`, `src/grant.cpp` | `announce()` and `equip()`: grants of an entry and the `CAPABILITY_GRANTED` message |
| `include/init/files.hpp`, `src/files.cpp` | `readInto()`: a whole file through the file client |
| `include/init/manifest.hpp`, `src/manifest.cpp` | `loadManifest()`: connect, read `manifest.conf`, parse it; holds the parsed `manifest` |
| `include/init/spawn.hpp`, `src/spawn.cpp` | `spawnProgram()`: reads the image into an anonymous mapping and calls `SYS_SPAWN` |
| `include/init/supervise.hpp`, `src/supervise.cpp` | `awaitExit()`: polls `PROCESS_EXITED` and `process_alive` |
| `include/init/run.hpp`, `src/run.cpp` | `runEntry()`: configure a boot module or spawn `repeat` times |
| `Makefile` | `NAME = init`, `SRCS`, a minimal `LIBC_SRCS` list, includes `../common.mk` |

## Build, run, test

```sh
make userland/init
make iso
make run-headless
bash scripts/qemu_userland_test.sh
```

On a machine without the cross compiler add `HOST_TOOLCHAIN=1`. The end to end script boots with the test initrd and requires these lines on the serial log: `init: initrd is boot module`, `init: manifest has 23 entries`, `init: configured vga_driver`, `init: started <name>` for each program and `init: fsclient exit notices 10/10`. The manifest parser has host tests in `test/host/test_manifest.cpp` (`bash scripts/host_test.sh`).

## Limits and known gaps

- A program image is at most 1 MiB and must fit the quota of `init`; the manifest is at most 4096 bytes and 32 entries.
- The capability policy is a text file read from the initrd: there is no revocation, no IPC capability and no DMA policy yet (stage 9 of the roadmap).
- Entries run in file order and a `repeat=` entry blocks the rest until its runs end. There is no restart of a server that dies.
- `init` exits when it is done, so nothing supervises the servers afterwards.

## Documentation

- [Userland programs, manifest and API](../../documentation/userland/userland.md)
- [File system boot sequence](../../documentation/userland/filesystem.md)
- [Capabilities](../../documentation/core/capabilities.md)
- [Task management and spawn](../../documentation/core/task_management.md)
