# fat

The FAT32 server. It serves one FAT32 volume of the disk with the common file protocol, so a client reaches it through `vfs` as `/fat/...`. It reads and writes, keeps long file names, and gets its file times from the `rtc` server.

## Where it sits

```text
vfs --FS_*--> fat --BLK_*--> ata
                \--RTC_READ--> rtc
```

- It is spawned from the initrd by `init`: `fat path=bin/fat.elf arg=2` in `initrd/manifest.conf`. It has no capabilities; the grant message only carries the block device in its second argument (device 0 is the whole disk, partitions follow). The test disk has its FAT32 volume on device 2.
- It is a client of `ata` (`cock::user::block::Client`) and of `rtc` (`cock::user::Clock`) and a server of the file protocol (`cock::user::FileServer<Fat>`). Messages that arrive while it waits for one of those replies are queued by `Server::defer`.
- `vfs` mounts it with the line `/fat fat` of `initrd/mounts.conf`.
- It waits for the grant message first. A manifest line without `arg=` leaves it waiting. On success it prints `fat: device <n>, <clusters> clusters of <sectors> sectors`; on failure `fat: device <n> is not a FAT32 volume` and exit status 1.

## Protocol

It serves the same messages as `ramfs` with writing enabled (`shared/include/cock/abi/fs.hpp`): `FS_CONNECT`, `FS_OPEN` (flags `OPEN_WRITE`, `OPEN_CREATE`, `OPEN_TRUNCATE`), `FS_READ`, `FS_WRITE`, `FS_UNLINK`, `FS_MKDIR`, `FS_RMDIR`, `FS_RENAME`, `FS_STAT`, `FS_READDIR`, `FS_CLOSE`, replies `FS_REPLY`. `FS_LINK` answers `UNSUPPORTED` because FAT has no hard links. Sessions, handles, windows, validation and the exclusivity rules (one writer or many readers, `BUSY` otherwise) come from `userland/lib/include/cock/user/file_server.hpp`; this server only supplies the backend: how to resolve a path, read and write bytes, create, truncate, unlink, list, rename and make directories.

Block protocol it uses: `BLK_CONNECT`, `BLK_INFO`, `BLK_READ`, `BLK_WRITE` (`shared/include/cock/abi/block.hpp`). Parsers shared with the host tests: `shared/include/cock/abi/fat32.hpp` (boot sector, entries, long names, UTF-8, 8.3 names). Date arithmetic: `shared/include/cock/abi/datetime.hpp`.

## Behaviour

- **Mount check.** The boot sector must pass `parseBpb` (512 byte sectors, signature, FAT32 shape, 65525 to `0x0FFFFFF4` clusters, room for the FAT). Only the first FAT is read; the FSInfo hints are kept and written back.
- **Names.** Lookup ignores ASCII case, uses the long name when its checksum matches and the short name otherwise, and long names are UTF-8 on the protocol. Names that fail 8.3 get long entries and an alias such as `LONGNA~1.TXT`; control characters and `\ / : * ? " < > |` give `BAD_REQUEST`.
- **Writes.** FAT copies are all updated; a crash leaves lost clusters that `fsck` reclaims, never an entry pointing at free space. The clean shutdown bit is cleared during an operation and set after; a volume found unclean, and the whole-disk device, are served read only.
- **Hostile images.** Every cluster number is checked before it is followed, chains and directory walks are bounded, sizes are clamped; a bad chain gives `IO_ERROR`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `_start`: wait for the grant, mount, run the server |
| `include/fat/grant.hpp`, `src/grant.cpp` | `waitForCapabilities()`: waits for `CAPABILITY_GRANTED`, queues other messages with `Server::defer`, returns the device |
| `include/fat/mount.hpp`, `src/mount.cpp` | `mountVolume()`: connects to `ata` and `rtc`, checks the boot sector, decides read only |
| `include/fat/volume.hpp`, `src/volume.cpp` | Geometry, sector buffers, `Spot`, `Cursor`, `Found`, `Node`, `Handle` |
| `include/fat/table.hpp`, `src/table.cpp` | FAT sectors, cluster links, allocate and free chains |
| `include/fat/fsinfo.hpp`, `src/fsinfo.cpp` | FSInfo counters and hint |
| `include/fat/state.hpp`, `src/state.cpp` | Clean shutdown bit: `isCleanVolume`, `beginUpdate`, `endUpdate` |
| `include/fat/stamp.hpp`, `src/stamp.cpp` | File times from `rtc`, 1980 epoch without it |
| `include/fat/directory.hpp`, `src/directory.cpp` | Walk, find, list, write and delete entries, empty check |
| `include/fat/entries.hpp`, `src/entries.cpp` | Short names, long entries, free runs: `placeEntry` |
| `include/fat/lookup.hpp`, `src/lookup.cpp` | Paths: `walk`, `resolve`, `parentAndName`, `isInside` |
| `include/fat/file.hpp`, `src/file.cpp` | File data: read, write, truncate |
| `include/fat/operations.hpp`, `src/operations.cpp` | Create, mkdir, rmdir, rename, unlink, write, truncate, each between `beginUpdate` and `endUpdate` |
| `include/fat/server.hpp` | `Backend` adapter and `Server = FileServer<Backend>` |
| `Makefile` | `NAME = fat`, `SRCS` and the `LIBC_SRCS` list |
| `../lib/include/cock/user/file_server.hpp` | The server side template shared with `ext` |
| `../lib/include/cock/user/block.hpp`, `clock.hpp` | The `ata` and `rtc` clients |

## Build, run, test

```sh
make userland/fat
make disk
make run-headless DISK=build/disk.img
DISK_SCHEME=gpt bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Add `HOST_TOOLCHAIN=1` without the cross compiler. `make disk` builds a disk with FAT32 and ext2 volumes (`scripts/make_test_disk.sh`, needs `sfdisk` and mtools); the shell then lists and writes `/fat`. The userland script runs `fatclient` and `cfile` against `/fat`, requires `fat: device`, `fatclient: ok` and `cfile: fat ok`, then runs `fsck.fat -n` on the partition and reads the surviving files back with `mtype` and `mdir`. Host tests: `test/host/test_fat32.cpp`, `test_datetime.cpp`.

## Limits and known gaps

- 512 byte sectors, one volume, file sizes up to 4 GiB less one, case folding for ASCII only.
- No hidden or read-only attributes, no hard or symbolic links, and the creation time is not kept apart from the write time on a rename.
- A write needs the `rtc` server for dates; without it new entries get 1980-01-01.
- No access control between clients until the IPC capability of stage 9.

## Documentation

- [FAT32 server](../../documentation/userland/fat32.md)
- [File system protocol](../../documentation/userland/filesystem.md)
- [Block storage](../../documentation/userland/block_storage.md)
- [Capabilities](../../documentation/core/capabilities.md)
