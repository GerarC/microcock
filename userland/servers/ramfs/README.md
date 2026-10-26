# ramfs

The file server of the initial ramdisk. It maps the `initrd`, a ustar archive loaded as a data boot module, and answers the common file protocol read only. It is the first file server to exist: `rs` reads `manifest.conf` through it, and `vfs` reads `mounts.conf` through it.

## Where it sits

```text
rs, vfs --FS_*--> ramfs --maps--> initrd boot module (ustar archive)
```

- It is a Multiboot module (`ramfs` is in `USERLAND_BOOT_ORDER`), not a file of the initrd, and it has no line in either manifest.
- Capability: `BOOT_MODULE` for the single module `initrd`, granted by `rs` in its own code (`SYS_CAP_GRANT`), followed by `CAPABILITY_GRANTED`. That is the only policy hard written in `rs`.
- Until the grant arrives, requests are kept (up to 16) and answered afterwards. After it, the server calls `SYS_MODULE_OPEN`, maps the module read only and builds the index. If either step fails it exits with status 1.
- `vfs` mounts it on `/` through `mounts.conf` (`/ ramfs`). Programs reach it by the path `/...` through `vfs`; only `rs` and `vfs` talk to it directly.
- It opts out of the console client (`CONSOLE_CLIENT = no`) because it starts before the console server.

## Protocol

Header: `shared/include/cock/abi/fs.hpp`. Every request carries a request id in `arg0`; the reply `FS_REPLY` echoes it, with the status in `arg1` and up to two values in `arg2` and `arg3`.

| Message | Served | Reply values |
|---------|--------|--------------|
| `FS_CONNECT` | yes | id of the shared window |
| `FS_OPEN` | read only flags; a directory can be opened (`/`, `docs`, `docs/`) | handle |
| `FS_READ` | yes, cut to the window | bytes copied into the window |
| `FS_STAT` | yes | size, type |
| `FS_READDIR` | direct children in archive order; `OUT_OF_RANGE` past the last | name length, type; the name is in the window |
| `FS_CLOSE` | yes | none |
| `FS_WRITE`, `FS_UNLINK`, `FS_MKDIR`, `FS_RMDIR`, `FS_RENAME`, `FS_LINK` | no | `READ_ONLY` |

Any other type is ignored, and so is anything from `KERNEL_SENDER` or without a reply token. A client that did not connect gets `NOT_CONNECTED`. The window is a one page shared memory object that the server creates, shares with the client and maps on `FS_CONNECT`; paths are copied out of it and a read never exceeds it. Sessions (8 clients, 16 handles each) are keyed by the badge of the handle (the process id of the client); dead clients are released when the table is full. The client library is `userland/lib/include/cock/user/fs.hpp`.

## The archive index

On start the server walks the tar headers once and keeps up to 128 entries (name, data offset, size, type) for regular files and directories. It stops at the first block that is empty, has no `ustar` magic or has a bad size field, and trailing slashes are removed from directory names. The archive may not exceed the shared object limit (`cock::abi::shm::MAX_OBJECT_BYTES`).

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `waitForCapabilities`, loads the archive, drains the kept messages, then the request loop |
| `src/grant.cpp`, `include/ramfs/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED`, keeping every other message |
| `src/dispatch.cpp` | `handle`: refuses unknown senders and requests without a session, answers writes with `READ_ONLY`, then calls one handler |
| `src/archive.cpp`, `include/ramfs/archive.hpp` | The tar archive: `loadArchive`, the index of entries, `findEntry`, `isChild` |
| `src/sessions.cpp`, `include/ramfs/sessions.hpp` | The sessions of the clients and their open files: `connect`, `releaseDeadSessions`, `handleEntry` |
| `src/requests.cpp`, `include/ramfs/requests.hpp` | `openFile`, `readFile`, `statFile`, `listDirectory`, `closeFile` |
| `src/pending.cpp`, `include/ramfs/pending.hpp` | `keepPending` and `handlePending`: requests that arrive before the grant |
| `src/reply.cpp`, `include/ramfs/reply.hpp` | `reply`: sends `FS_REPLY` |
| `Makefile` | `NAME = ramfs`, `SRCS` under `src/`, `CONSOLE_CLIENT = no` and a small `LIBC_SRCS` list |
| `../../lib/include/cock/user/session.hpp` | Session helpers shared with `vfs`, `fat`, `ext` and `ata` |
| `../../../initrd/` | The files packed into the archive (`etc/manifest.conf`, `etc/mounts.conf`) |

## Build, run, test

```sh
make userland/servers/ramfs
make iso
make run-headless
bash scripts/qemu_userland_test.sh
```

Add `HOST_TOOLCHAIN=1` without the cross compiler. The archive is made by the `Makefile` (`build/initrd.tar`; the `iso` rule also adds the test files with `TEST_INITRD=1`). The end to end script requires `rs: initrd is boot module`, the manifest parse, ten `fsclient: ok` lines (dead sessions are released) and the file checks of `probe` (`initrdFileReads`, `fileServerListsDirectories`, `fileServerRefusesBadRequests`).

## Limits and known gaps

- Read only. A writable memory file system (`tmpfs`) is noted in the roadmap; it is not here.
- At most 128 entries and an archive below the shared object limit; names up to 100 bytes (the tar name field), no `prefix` field and no long name extensions.
- No symbolic links, no hard links and no file times.
- No access control between clients until the IPC capability of stage 9.

## Documentation

- [File system](../../../documentation/userland/filesystem.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
- [Memory management and boot modules](../../../documentation/core/memory_management.md)
- [Capabilities](../../../documentation/core/capabilities.md)
