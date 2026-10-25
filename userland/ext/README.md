# ext

The ext2 server. It serves one ext2 volume of the disk with the common file protocol, so a client reaches it through `vfs` as `/ext/...`. It reads, follows symbolic links and, on a clean volume without a journal, writes.

## Where it sits

```text
vfs --FS_*--> ext --BLK_*--> ata
                \--RTC_READ--> rtc
```

- It is spawned from the initrd by `rs`: `ext path=bin/ext.elf arg=3` in `initrd/manifest.conf`. It has no capabilities; `arg=` is the block device it serves.
- The test manifest starts the same program a second time as `ext4probe` with `arg=4`, the ext4 volume of the test disk, to show that it is refused. The test boot therefore prints `ext: device 4: unsupported feature extents`.
- `vfs` mounts it with the line `/ext ext` of `initrd/mounts.conf`.
- It is a client of `ata` and of `rtc` and a server of the file protocol through `cock::user::FileServer<Ext>`, like `fat`. On success it prints `ext: device <n>, <blocks> blocks of <bytes> bytes, <inodes> inodes`; each failure prints why and the server exits with status 1.

## Protocol

It serves `FS_CONNECT`, `FS_OPEN` (with `OPEN_WRITE`, `OPEN_CREATE`, `OPEN_TRUNCATE`), `FS_READ`, `FS_WRITE`, `FS_UNLINK`, `FS_MKDIR`, `FS_RMDIR`, `FS_RENAME`, `FS_LINK`, `FS_STAT`, `FS_READDIR` and `FS_CLOSE` (`shared/include/cock/abi/fs.hpp`), answering `FS_REPLY`. File types include `SYMLINK`: a link is listed with that type and opens as its target. Sessions, handles, windows and the one writer or many readers rule come from `userland/lib/include/cock/user/file_server.hpp`. It uses the block protocol of `ata` (`shared/include/cock/abi/block.hpp`). On-disk parsers: `shared/include/cock/abi/ext2.hpp` (`parseSuperblock`, inodes, directory records, `blockPath`, feature names).

## Behaviour

- **Mount check.** Magic, block size (1024, 2048 or 4096), inode size (128 to 512), geometry, and an incompatible feature set that contains only `filetype`. Anything else, such as extents, `flex_bg`, `64bit` or `needs_recovery`, prints `ext: device N: unsupported feature <name>` and the volume is refused.
- **Reading.** Classic block map (12 direct, single, double and triple indirect); holes read as zeros; every block number is checked against the volume; symbolic links are followed up to `MAX_LINKS` (8) per path; `..` of the root is the root.
- **Writable** only when the read-only-compatible features are `sparse_super` and `large_file`, there is no journal and the state is `VALID` without `ERROR`. Otherwise the server prints `ext: device N is mounted read only` and writes answer `READ_ONLY`.
- **Writes.** Create, write, truncate, delete, `mkdir`, `rmdir`, rename and move, hard links (up to 65000). Each allocation is written through at once; the `VALID` flag is cleared during an operation and set after, and orderings are chosen so a crash leaves unreferenced blocks for `e2fsck`, never a name over free space. Times come from `rtc`, with the fixed mark 1000000000 without it.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: wait for the grant, mount, run the server |
| `include/ext/grant.hpp`, `src/grant.cpp` | `waitForCapabilities()`: waits for `CAPABILITY_GRANTED`, queues other messages with `Server::defer`, returns the device |
| `include/ext/mount.hpp`, `src/mount.cpp` | `mountVolume()`: connects to `ata` and `rtc`, checks the superblock and the root, reports refusals |
| `include/ext/volume.hpp`, `src/volume.cpp` | Superblock, sector buffers, `Node`, `Handle`, `Where`, `loadSector`, `storeSector` |
| `include/ext/stamp.hpp`, `src/stamp.cpp` | Times from `rtc`, the fixed mark without it |
| `include/ext/state.hpp`, `src/state.cpp` | `VALID` flag and free counters: `beginUpdate`, `endUpdate` |
| `include/ext/inode.hpp`, `src/inode.cpp` | Group descriptors, `loadInode`, `storeInode` |
| `include/ext/alloc.hpp`, `src/alloc.cpp` | Block and inode bitmaps: allocate, free, group counters |
| `include/ext/blockmap.hpp`, `src/blockmap.cpp` | Direct and indirect blocks: map, grow, release, empty a file |
| `include/ext/file.hpp`, `src/file.cpp` | File data: read and write |
| `include/ext/directory.hpp`, `src/directory.cpp` | Directory records: find, list, add, remove, empty check |
| `include/ext/lookup.hpp`, `src/lookup.cpp` | Paths with symbolic links, `parentAndName`, `isAncestor` |
| `include/ext/operations.hpp`, `src/operations.cpp` | Create, truncate, unlink, mkdir, rmdir, rename, link, write, each between `beginUpdate` and `endUpdate` |
| `include/ext/server.hpp` | `Backend` adapter and `Server = FileServer<Backend>` |
| `Makefile` | `NAME = ext`, `SRCS` and the `LIBC_SRCS` list |
| `../lib/include/cock/user/file_server.hpp` | Server template shared with `fat` |
| `../../shared/include/cock/abi/ext2.hpp` | Format parsers and constants, covered by host tests |

## Build, run, test

```sh
make userland/ext
make disk
make run-headless DISK=build/disk.img
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Add `HOST_TOOLCHAIN=1` without the cross compiler. `make disk` needs `mke2fs` 1.43 or later and `sfdisk`. The userland script runs `extclient` and `cfile` against `/ext`, requires `ext: device`, `extclient: ok`, `cfile: ext ok` and the refusal of `ext4probe`, then runs `e2fsck -fn` on the partition and reads the surviving files back with `debugfs`. The shell test also writes, moves and deletes a file in `/ext/scratch`. Host tests: `test/host/test_ext2.cpp`.

## Limits and known gaps

- 512 byte sectors on the device, block sizes up to 4096, inodes up to 512 bytes, no extents, no extended attributes or ACLs.
- Files of 2 GiB or more are cut on read and never written; the protocol does not return owner, mode or times.
- Symbolic links are read but never created (postponed by the user); an absolute link target starts at the root of this volume.
- Backup superblocks and group descriptors are not updated and no blocks are reserved for root.
- No access control between clients until the IPC capability of stage 9.

## Documentation

- [ext2 server](../../documentation/userland/ext2.md)
- [File system protocol](../../documentation/userland/filesystem.md)
- [Block storage](../../documentation/userland/block_storage.md)
- [Capabilities](../../documentation/core/capabilities.md)
