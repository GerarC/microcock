# File system

The file system is not in the kernel. `ramfs` is a Ring 3 server that serves the files of the `initrd` (a ustar archive loaded as a data boot module, see [memory management](memory_management.md)) to other programs by IPC and shared memory. It is read only.

## Boot
1. The kernel keeps `initrd` as a data module and loads `ramfs` like any other program.
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
| `FS_CLOSE` | `arg1` handle | none |

Statuses: `OK`, `NOT_CONNECTED`, `BAD_REQUEST`, `NOT_FOUND`, `BAD_HANDLE`, `OUT_OF_RANGE`, `NO_SPACE`, `UNSUPPORTED`.

### Window
Paths and data never travel in the message. On `FS_CONNECT` the **server** creates a one page shared memory object, shares it with the client with write access, maps it and replies with its id; the client maps it. The server owns the size, so a client cannot make it read past the end of a window. The path of `FS_OPEN` is copied out of the window once before it is used, and a leading `/` is accepted. The data of `FS_READ` is copied to the start of the window and a read never exceeds the window.

### Sessions and handles
The server keeps up to 8 sessions keyed by the kernel stamped `sender_pid`, each with 16 handles. A handle is the slot number plus one, valid only in the session that opened it. A second `FS_CONNECT` from the same process resets its handles and returns the same window. Sessions are not freed when a client dies yet, so the table is bounded by 8 clients over the life of the server.

## Client library
`userland/lib/include/cock/user/fs.hpp` offers `cock::user::fs::Client` with `connect`, `open`, `read`, `stat`, `close` and the raw `call`. A program that uses it lists `string/memcpy.cpp` and `string/strlen.cpp` in `LIBC_SRCS`.

## Tests
`probe` checks that the file of the initrd reads back whole and in pieces (`initrdFileReads`, which also prints it), that the server refuses bad paths, handles, offsets, flags and lengths (`fileServerRefusesBadRequests`), and that a program without authority cannot call `SYS_MODULE_INDEX` (`moduleIndexIsDenied`).
