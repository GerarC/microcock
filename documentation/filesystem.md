# File system

The file system is not in the kernel. `ramfs` is a Ring 3 server that serves the files of the `initrd` (a ustar archive loaded as a data boot module, see [memory management](memory_management.md)) to other programs by IPC and shared memory. It is read only.

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

Statuses: `OK`, `NOT_CONNECTED`, `BAD_REQUEST`, `NOT_FOUND`, `BAD_HANDLE`, `OUT_OF_RANGE`, `NO_SPACE`, `UNSUPPORTED`.

### Window
Paths and data never travel in the message. On `FS_CONNECT` the **server** creates a one page shared memory object, shares it with the client with write access, maps it and replies with its id; the client maps it. The server owns the size, so a client cannot make it read past the end of a window. The path of `FS_OPEN` is copied out of the window once before it is used, and a leading `/` is accepted. The data of `FS_READ` is copied to the start of the window and a read never exceeds the window.

### Sessions and handles
The server keeps up to 8 sessions keyed by the kernel stamped `sender_pid`, each with 16 handles. A handle is the slot number plus one, valid only in the session that opened it. A second `FS_CONNECT` from the same process resets its handles and returns the same window. When a client asks to connect and the table is full, the server asks `SYS_PROC_ALIVE` about every session, unmaps the window of each dead client, releases its shared memory object and frees the slot. A client that dies leaves its session until then. The process id carries a generation, so a recycled slot is never mistaken for the old client.

## Programs from the initrd
`init` is the first client. It reads `manifest.conf`, and for each program of the manifest it opens the file, reads it into an anonymous mapping through the window and calls `SYS_SPAWN`; see [the manifest](userland.md#the-manifest) and [task management](task_management.md#spawn). `fsclient` is a small client that the manifest runs ten times to exercise the release of sessions.

## Client library
`userland/lib/include/cock/user/fs.hpp` offers `cock::user::fs::Client` with `connect`, `open`, `read`, `readdir`, `stat`, `close` and the raw `call`. A program that uses it lists `string/memcpy.cpp` and `string/strlen.cpp` in `LIBC_SRCS`.

## Tests
`probe` checks that the file of the initrd reads back whole and in pieces (`initrdFileReads`, which also prints it), that `fileServerListsDirectories` lists the root and a subdirectory and reads a nested file, that the server refuses bad paths, handles, offsets, flags and lengths (`fileServerRefusesBadRequests`), and that a program without authority cannot call `SYS_MODULE_INDEX` (`moduleIndexIsDenied`). The success path of `SYS_MODULE_INDEX` runs at every boot: `init` needs it to grant `ramfs` the `initrd`, and the end to end test requires the line `init: initrd is boot module`. `probe` also reads the manifest through the client, parses it and checks that its own entry has no grants (`manifestMatchesTheBoot`). The test also requires ten `fsclient: ok` lines, which only happen if dead sessions are released.
