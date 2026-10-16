# cock::user library

The C++ library every Ring 3 program of MicroCOCK includes. It is header only (`userland/lib/include/cock/user/`) and has no sources and no Makefile: each program compiles what it includes, with `-I` set by `userland/common.mk`. It wraps system calls, holds the clients of the servers and holds the server side of the file protocol.

## Where it sits

```text
programs (init, sh, ata, fat, ext, vfs, ...) --> cock::user --> system call wrapper of libc
                                                          \--> IPC to console, vfs, ata, rtc
```

- Message layouts and protocol constants are not here: they live in `shared/include/cock/ipc/message.hpp` and `shared/include/cock/abi/*.hpp`, so the kernel, the host tests and the programs agree on them.
- The library needs no capability. What a call may do depends on the capabilities the caller holds (see the manifests in `initrd/manifest.conf` and `test/initrd/manifest.conf`).
- It sits above the freestanding C library (`libc/`), which a program lists file by file in `LIBC_SRCS`; `common.mk` adds the console client files unless the program sets `CONSOLE_CLIENT = no`.

## Headers

| Header | Holds |
|--------|-------|
| `api.hpp` | System call wrappers: `ipc_send`, `ipc_recv`, `ipc_try_recv`, `ipc_recv_timeout`, `sleep_ms`, `request_ports`, `irq_bind`, `irq_ack`, `cap_grant`, `lookup`, `shm_create`, `shm_share`, `shm_map`, `shm_release`, `module_open`, `module_index`, `spawn`, `process_alive`, `gettid`, `futex_wait`, `futex_wake`, port I/O, `write_text`, `exit_process`; the stash of messages that other waits received |
| `call.hpp` | Synchronous calls with four words in registers: `ipc_call`, `ipc_accept`, `ipc_reply`, `ipc_reply_wait`, `ipc_defer`, `ipc_reply_to`; `CallStatus` and `CallResult` |
| `thread.hpp`, `mutex.hpp` | `thread_start` on a caller supplied stack, and a futex mutex for threads of one process |
| `fs.hpp` | `fs::Client`: `connect`, `open`, `read`, `write`, `unlink`, `mkdir`, `rmdir`, `rename`, `link`, `readdir`, `stat`, `close`, `lastStatus` |
| `file_server.hpp` | `FileServer<Backend>`: sessions, handles, windows, validation and replies for `fat` and `ext` |
| `session.hpp`, `reply.hpp` | Session helpers (`findSession`, `freeSession`, `openSession`, `releaseSession`) and `receiveReply`, which waits with a timeout and gives up when the server is gone |
| `block.hpp` | `block::Client` for `ata`: `connect`, `deviceCount`, `info`, `read`, `write` |
| `console.hpp` | `console::Client` for `console`: `connect`, `write`, `read`, `clear` |
| `clock.hpp`, `cmos.hpp` | `Clock`, the client of `rtc`, and `Cmos`, the port reader that only `rtc` uses |

## Protocols it implements

- File protocol (`FS_CONNECT` to `FS_CLOSE`, reply `FS_REPLY`): client in `fs.hpp`, server in `file_server.hpp`. The server creates a one page window per client and owns its size.
- Block protocol (`BLK_CONNECT`, `BLK_INFO`, `BLK_READ`, `BLK_WRITE`, reply `BLK_REPLY`): client in `block.hpp`.
- Console protocol (synchronous calls with `WRITE`, `READ`, `CLEAR`): client in `console.hpp`.
- Clock protocol (`RTC_READ`, `RTC_REPLY`): client in `clock.hpp`.

A client that is also a server (`vfs`, `fat`, `ext`) sets `onForeign`, a function that receives the messages that arrive while it waits for a reply; they must be kept, not dropped.

## How to use it

A program has a folder in `userland/` with a `Makefile` that sets `NAME`, `SRCS` and `LIBC_SRCS` and includes `../common.mk`, an entry `_start` declared `extern "C"` that ends with `exit_process`, and one line in `initrd/manifest.conf`. Programs that use the file client list `string/memcpy.cpp` and `string/strlen.cpp` in `LIBC_SRCS`.

```sh
make userland/sh
make userland
make iso
```

Add `HOST_TOOLCHAIN=1` without the cross compiler, and `USER_EXTRA_FLAGS=-fno-omit-frame-pointer` to check the system call wrapper with frame pointers. The headers are exercised by every program of `userland/` and `test/userland/`; `bash scripts/qemu_userland_test.sh` boots them together, and `bash scripts/host_test.sh` tests the shared parsers they depend on.

## Limits and known gaps

- Not thread safe: `fs::Client`, `Clock` and the stash keep state per program; `malloc` of the C library is locked and safe for threads.
- Clients look the server up by name and have no access control: any process may talk to `console`, `vfs`, `ata` or `rtc` until the IPC capability of stage 9.
- Wait times have a resolution of the kernel tick (10 ms for `sleep_ms`).
- The wrappers keep the stash of 16 messages; more are dropped.
- There is no x86_64 variant yet: the wrappers assume the `int 0x80` ABI of i386.

## Documentation

- [Userland programs, API and ABI](../../documentation/userland/userland.md)
- [File system client library](../../documentation/userland/filesystem.md)
- [Console client](../../documentation/userland/console.md)
- [Block storage client](../../documentation/userland/block_storage.md)
- [IPC](../../documentation/core/ipc.md)
- [Task management and threads](../../documentation/core/task_management.md)
