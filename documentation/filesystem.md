# File system

The file system is not in the kernel. It is a set of Ring 3 servers that talk by IPC and shared memory, all with the same protocol: `ramfs` serves the files of the `initrd` (a ustar archive loaded as a data boot module, see [memory management](memory_management.md)), `fat` serves a FAT32 volume of the disk (see [FAT32](fat32.md)), and `vfs` is the one name clients use: it forwards each request to the server that owns the path. Everything is read only.

## Boot
1. The kernel keeps `initrd` as a data module and loads `ramfs` as a boot module; every other program is a file of the archive under `bin/`.
2. `init` asks `SYS_MODULE_INDEX` for the index of `initrd` (only a process with grant authority may ask), grants `ramfs` a `BOOT_MODULE` capability for that one module and sends it `CAPABILITY_GRANTED`.
3. `ramfs` opens the module, maps it read only and walks the tar headers once into an index of up to 64 entries (name, data offset, size, type). Requests that arrive before the grant are kept (up to 16) and answered afterwards.

## Protocol
Every request carries a request id in `arg0`; the reply (`FS_REPLY`) echoes it in `arg0` and carries the status in `arg1` and up to two values in `arg2` and `arg3`. A client accepts a reply only from the server pid and with its own id. The constants are in `shared/include/cock/abi/fs.hpp`.

| Message | Arguments | Reply values |
|---------|-----------|--------------|
| `FS_CONNECT` | none | id of the window |
| `FS_OPEN` | `arg1` path length, `arg2` flags (read only is 0) | handle |
| `FS_READ` | `arg1` handle, `arg2` offset, `arg3` length | bytes copied into the window (0 at the end of the file) |
| `FS_STAT` | `arg1` handle | size, type |
| `FS_READDIR` | `arg1` handle of a directory, `arg2` index | name length, type (the name is in the window, ended by a zero) |
| `FS_CLOSE` | `arg1` handle | none |

`FS_OPEN` accepts a directory: `/` is the root and `docs` or `docs/` a subdirectory. `FS_READDIR` lists the direct children of a directory in archive order and answers `OUT_OF_RANGE` past the last one; on a regular file it answers `UNSUPPORTED`, as `FS_READ` does on a directory.

Statuses: `OK`, `NOT_CONNECTED`, `BAD_REQUEST`, `NOT_FOUND`, `BAD_HANDLE`, `OUT_OF_RANGE`, `NO_SPACE`, `UNSUPPORTED`, `IO_ERROR` (the disk failed; only `fat` answers it).

### Window
Paths and data never travel in the message. On `FS_CONNECT` the **server** creates a one page shared memory object, shares it with the client with write access, maps it and replies with its id; the client maps it. The server owns the size, so a client cannot make it read past the end of a window. The path of `FS_OPEN` is copied out of the window once before it is used, and a leading `/` is accepted. The data of `FS_READ` is copied to the start of the window and a read never exceeds the window.

### Sessions and handles
The server keeps up to 8 sessions keyed by the kernel stamped `sender_pid`, each with 16 handles. A handle is the slot number plus one, valid only in the session that opened it. A second `FS_CONNECT` from the same process resets its handles and returns the same window. When a client asks to connect and the table is full, the server asks `SYS_PROC_ALIVE` about every session, unmaps the window of each dead client, releases its shared memory object and frees the slot. A client that dies leaves its session until then. The process id carries a generation, so a recycled slot is never mistaken for the old client.

## The `vfs` server
`vfs` is spawned from the initrd by `init` and needs no capabilities. Clients connect to it by the name `vfs` (`abi::fs::SERVER_NAME`); only `init` talks to `ramfs` directly, because it has to read the manifest before `vfs` exists.

- **Mount table.** Read at start from `mounts.conf` in the initrd (through `ramfs`, by name, like `init` reads the manifest; the parser is `shared/include/cock/abi/mounts.hpp`). One line per mount, a prefix and the name of the server: `/ ramfs`, `/fat fat`, `/mute mute`. Empty lines and lines that start with `#` are ignored. A prefix starts with `/`, has no trailing `/` and no empty component; there are at most 8 mounts, the prefixes are unique and `/` must be one of them. A bad file makes `vfs` print the line and the error and exit. The device a server uses is the argument of its line in the manifest (`arg=`), not part of the mount. A path is made absolute (a leading `/` is added when missing), the longest prefix that ends at a component boundary wins (`/fatx` belongs to `/`, not to `/fat`), the prefix is removed and the rest is sent to the server (`/fat` alone becomes `/`).
- **Proxy.** `vfs` is itself a client of each server, with its own window, which it connects to the first time the mount is used (a mount whose server is not running answers `NOT_FOUND`). The data of a read goes from the window of the server straight into the window of the client, one copy. Statuses from the server are passed on unchanged, so a client sees the same refusals whichever server owns the file.
- **Handles.** A handle is the slot number plus one in a table of 16 open files shared by all clients (each server allows 16 handles to `vfs`), and it is only valid for the client that opened it. When the table is full, or a client connects again, or a session slot is needed, `vfs` closes the files of dead clients (`SYS_PROC_ALIVE`) at the server and frees their sessions.
- **One request at a time.** While `vfs` waits for a server it keeps the requests that arrive in a queue of 16 and handles them afterwards, so another client is never lost.
- **A server that dies.** The clients wait for a reply with `SYS_IPC_TRY_RECV` and, while the inbox is empty, ask `SYS_PROC_ALIVE` about the server and yield; when the server is gone the call fails instead of waiting forever. `vfs` then marks the mount as down, forgets the open files of that mount (their handles belonged to the dead server) and answers `NOT_CONNECTED`; the next request to that mount looks the server up again, so a server that was started again is used without restarting `vfs`. Waiting costs a yield per poll, not a sleep, until there are timers.
- **The root listing** (`/`) shows the entries of the file system mounted on `/` and then one directory for every mount whose prefix has a single component (`fat`, `mute`). Mounts deeper than that are reachable by name only.
- **Not yet.** Changing the table needs a new `mounts.conf` in the initrd; no mount or unmount at run time.

## Programs from the initrd
`init` is the first client. It reads `manifest.conf`, and for each program of the manifest it opens the file, reads it into an anonymous mapping through the window and calls `SYS_SPAWN`; see [the manifest](userland.md#the-manifest) and [task management](task_management.md#spawn). `fsclient` is a small client that the manifest runs ten times to exercise the release of sessions; it goes through `vfs`, so the release is exercised there too. `mute` is a server that reads one message and exits without answering, and `muteclient` checks that a request to it through `vfs` fails instead of hanging, that the file system still answers afterwards, and that a direct client of the dead server also fails.

## Client library
`userland/lib/include/cock/user/fs.hpp` offers `cock::user::fs::Client` with `connect(name)` (the default is `vfs`), `open`, `read`, `readdir`, `stat`, `close`, `lastStatus()` (the status of the last call, `NOT_CONNECTED` when the call did not go through) and the raw `call`. A server that is a client of another sets `onForeign`, a function that receives the messages that arrive while it waits for a reply; `vfs` queues them. A reply is awaited with `receiveReply` (`userland/lib/include/cock/user/reply.hpp`), which polls `SYS_IPC_TRY_RECV` and gives up when the server is no longer alive. `userland/lib/include/cock/user/session.hpp` has the session helpers the four servers share (`findSession`, `freeSession`, `openSession`, `releaseSession`). A program that uses it lists `string/memcpy.cpp` and `string/strlen.cpp` in `LIBC_SRCS`.

## Tests
`probe` checks that the file of the initrd reads back whole and in pieces (`initrdFileReads`, which also prints it), that `fileServerListsDirectories` lists the root and a subdirectory and reads a nested file, that the server refuses bad paths, handles, offsets, flags and lengths (`fileServerRefusesBadRequests`), and that a program without authority cannot call `SYS_MODULE_INDEX` (`moduleIndexIsDenied`). The success path of `SYS_MODULE_INDEX` runs at every boot: `init` needs it to grant `ramfs` the `initrd`, and the end to end test requires the line `init: initrd is boot module`. `probe` also reads the manifest through the client, parses it and checks that its own entry has no grants (`manifestMatchesTheBoot`). The test also requires ten `fsclient: ok` lines, which only happen if dead sessions are released.
