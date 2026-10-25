# vfs

The one file system name clients use. It reads a mount table, picks the server that owns each path, removes the prefix and forwards the request. Statuses come back unchanged, so a client sees the same refusals whichever server owns the file.

## Where it sits

```text
sh, init clients, test programs --FS_*--> vfs --FS_*--> ramfs, fat, ext, mute
```

- It is spawned from the initrd by `init` (`vfs path=bin/vfs.elf` in `initrd/manifest.conf`). It needs no capabilities and receives no `CAPABILITY_GRANTED`.
- At start it connects to `ramfs` by name, reads `mounts.conf` from the initrd and prints `vfs: <n> mounts`. A missing or bad file prints `vfs: cannot read mounts.conf` or the line and error and exits with status 1.
- The mount table of the system is `/ ramfs`, `/fat fat` and `/ext ext` (`initrd/mounts.conf`). The test initrd appends `/mute mute` (`test/initrd/mounts.conf`), so the test boot prints `vfs: 4 mounts`.
- The block device each server uses is the `arg=` of its manifest line, not part of the mount.

## Protocol

It speaks the same protocol on both sides (`shared/include/cock/abi/fs.hpp`): `FS_CONNECT`, `FS_OPEN`, `FS_READ`, `FS_WRITE`, `FS_UNLINK`, `FS_MKDIR`, `FS_RMDIR`, `FS_RENAME`, `FS_LINK`, `FS_STAT`, `FS_READDIR`, `FS_CLOSE`, answered with `FS_REPLY` (request id in `arg0`, status in `arg1`, values in `arg2` and `arg3`). Mount table grammar and parser: `shared/include/cock/abi/mounts.hpp`.

- **Routing.** A path is made absolute; the longest prefix that ends at a component boundary wins (`/fatx` belongs to `/`, not to `/fat`); the prefix is removed (`/fat` alone becomes `/`).
- **Proxy.** `vfs` is a client of each server (`cock::user::fs::Client`) with its own window, connected the first time the mount is used. A mount whose server is not running answers `NOT_FOUND`. Read data moves from the window of the server into the window of the client with one copy.
- **Rename and link across mounts** answer `CROSS_DEVICE` without asking a server.
- **Handles.** A table of 16 open files is shared by all clients; a handle is only valid for the client that opened it. When the table or the sessions are full, or a client connects again, `vfs` closes the files of dead clients (`SYS_PROC_ALIVE`) at the server and frees their sessions. Clients get up to 8 sessions with a one page window.
- **Root listing.** `/` shows the entries of the file system on `/` and one directory for each mount whose prefix has a single component (`fat`, `ext`, `mute`).
- **One request at a time.** While `vfs` waits for a server, other requests wait in a queue of 16 and are handled afterwards.
- **A server that dies.** Replies are awaited with `SYS_IPC_RECV_TIMEOUT` (20 ms) and `SYS_PROC_ALIVE`; when the server is gone the mount is marked down, its open files are forgotten and the client gets `NOT_CONNECTED`. The next request looks the server up again, so a restarted server is used without restarting `vfs`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: loads the mount table, then the request loop |
| `src/dispatch.cpp` | `handle`: refuses the kernel, requests without a reply token and requests without a session, then calls one handler |
| `src/mounts.cpp`, `include/vfs/mounts.hpp` | The mount table: `loadMounts`, `pickMount`, `ensureMount`, `markDown`, `failure`, `isMountPoint` |
| `src/opens.cpp`, `include/vfs/opens.hpp` | The table of open files: `findOpen`, `freeOpen`, `closeOpen`, `closeOwnedBy` |
| `src/clients.cpp`, `include/vfs/clients.hpp` | The sessions of the clients: `connect`, `releaseDead` |
| `src/routing.cpp`, `include/vfs/routing.hpp` | `route`: a path in a client window becomes a mount and a path inside it |
| `src/file_requests.cpp` | `openFile`, `readFile`, `writeFile`, `statFile`, `closeFile` |
| `src/path_requests.cpp` | `pathRequest` (mkdir and rmdir), `unlinkFile`, `renameEntry`, `linkEntry` |
| `src/directory_requests.cpp` | `listDirectory` and the mounts shown as directories in `/` |
| `src/pending.cpp`, `include/vfs/pending.hpp` | The queue of requests that arrive while a backend server is awaited |
| `src/reply.cpp`, `include/vfs/reply.hpp` | `reply`: sends `FS_REPLY` |
| `include/vfs/limits.hpp`, `include/vfs/requests.hpp` | Shared limits and path constants; the declaration of every handler |
| `Makefile` | `NAME = vfs`, the `SRCS` under `src/` and the `LIBC_SRCS` list (printf and string functions) |
| `../lib/include/cock/user/fs.hpp` | `fs::Client`, used by `vfs` and by every program |
| `../lib/include/cock/user/reply.hpp` | `receiveReply`, the wait with liveness checks |
| `../lib/include/cock/user/session.hpp` | Session helpers |

## Build, run, test

```sh
make userland/vfs
make iso TEST_INITRD=1
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Add `HOST_TOOLCHAIN=1` without the cross compiler. The end to end script requires `vfs: 4 mounts`, the output of `fsclient`, `fatclient`, `extclient`, `cfile` and `muteclient` (a dead `mute` server must not hang the proxy), and the shell commands that cross the mounts. The mount table parser has host tests in `test/host/test_mounts.cpp` and the path splitter in `test/host/test_fs.cpp`.

## Limits and known gaps

- The table is fixed at boot: at most 8 mounts, prefixes up to 64 bytes, no mount or unmount at run time; a change needs a new `mounts.conf` in the initrd.
- At most 16 open files and 8 clients for all mounts together.
- Requests are handled one at a time, so a slow disk server delays every client.
- Rename and hard links between mounts are refused; there is no copy.
- No symbolic links (postponed) and no access control between clients until the IPC capability of stage 9.

## Documentation

- [File system](../../documentation/userland/filesystem.md)
- [FAT32 server](../../documentation/userland/fat32.md)
- [ext2 server](../../documentation/userland/ext2.md)
- [Userland programs and the manifest](../../documentation/userland/userland.md)
