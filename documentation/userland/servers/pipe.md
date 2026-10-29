# The pipe server

A pipe connects a writer and a reader with a queue of bytes: what one process writes the other reads, in order, and the reader sees the end of the data when every write end is closed. In Unix the kernel holds it. Here it is the `pipe` server, a Ring 3 program. Does it have to live in the kernel? No: it needs no privileged instruction, no page table and no scheduler decision, only memory and the synchronous calls that already exist, and the kernel keeps no table of descriptors. The kernel part is what was already there: calls through an endpoint handle, the badge that names the caller, and `SYS_IPC_DEFER` to hold a caller.

| Part | Where |
|------|-------|
| Protocol | `shared/include/cock/abi/pipe.hpp` |
| Client | `userland/lib/include/cock/user/pipe.hpp` |
| Server | `userland/servers/pipe` (see its [README](../../../userland/servers/pipe/README.md)) |
| `pipe()` for programs | `libc/unistd/pipe.cpp`, over the table of [descriptors](../../libs/descriptors.md) |
| Test | `test/userland/pipeclient` (`pipeclient: ok`) |

## Ends
A pipe has a **read end** and a **write end**. An end belongs to the process that holds it and has a number that is local to that process, like a descriptor, so `pipe()` in two processes can give the same number to different ends. The server finds an end by the badge of the caller (its process id) and the number, so a process cannot use an end it was not given, and it cannot guess another process's number into use. A description of the C library that names an end closes it when the last descriptor that names it closes (`dup` and `dup2` share it).

## Protocol
Every request is a synchronous call of four words through the handle on the `pipe` endpoint that the name server gives (`cock::abi::pipe::SERVER_NAME`, `"pipe"`; a program lists `pipe` in `ipc=`). Word 0 holds the operation in bits 0 to 7, a byte count in bits 8 to 15 and the number of the end in bits 16 to 31; words 1 to 3 carry up to 12 bytes, as the console does. The reply has the status in bits 0 to 7 and a count in bits 8 to 15.

| Operation | Request | Reply |
|-----------|---------|-------|
| `CREATE` | nothing | `OK` with the read end in word 1 and the write end in word 2 |
| `READ` | the read end and the most bytes wanted, 1 to 12 | `OK`, the count and the bytes; held while the pipe is empty and a write end is open; a count of 0 means the end of the data |
| `WRITE` | the write end and 1 to 12 bytes | `OK` and the bytes taken, which can be fewer than sent; held while the pipe is full; `BROKEN` when no read end is open |
| `CLOSE` | the end | `OK` |

Other statuses: `BAD_REQUEST` (unknown operation, length 0 or above 12), `BAD_HANDLE` (the caller has no such end, or it is the other kind), `NO_ROOM` (the tables are full), `BUSY` (a call that had to wait could not be held; the client sleeps 10 ms and asks again). `NOT_CONNECTED` is never sent: it is what the client reports when its call did not go through.

## What the server does
* **Waiting.** When a `READ` finds the pipe empty, or a `WRITE` finds it full, the server detaches the call with `SYS_IPC_DEFER` and keeps the request in a table of 16 waiting callers. After every call it tries the waiting requests again, in table order, and answers each one that can now be served with `SYS_IPC_REPLY_TO`; it repeats while an answer frees something for another waiter. The waiter is blocked in the kernel and uses no CPU.
* **End of the data.** A reader gets 0 when the pipe is empty and `writers` is 0. Closing the last write end therefore wakes every blocked reader with 0, and closing the last read end wakes every blocked writer with `BROKEN`.
* **Dead processes.** A process that exits without closing leaves its ends open. While any end exists the server waits for a call with a limit of 50 ms; when the limit runs out it closes the ends of processes that are no longer alive (`SYS_PROC_ALIVE`), drops their waiting calls and tries the rest again, so a reader whose writer died sees the end of the data within about 50 ms. A full table also triggers that sweep before it answers `NO_ROOM`.
* **Size.** 16 pipes of 512 bytes, 64 ends for all processes, 16 waiting callers.

## The C library
`pipe(fds)` connects to the server on first use, makes a pipe and gives the lowest two free descriptors to a read description and a write description. `read` returns what one call brings (up to 12 bytes; a short read is allowed), `write` repeats until everything is written, `lseek` fails with `ESPIPE`. Errors: `EPIPE` for `BROKEN`, `ENFILE` for `NO_ROOM`, `EBADF` for `BAD_HANDLE`, `ENOTCONN` when the server cannot be reached (and then every pipe description is dropped, as for files).

## Limits and what is not done
* A child made by `fork` gets twins of the ends of its parent under the same numbers (`notify=fork`: `rs` sends the server `PM_FORKED`, and the server drains its inbox before it serves the next call, so the copy is made before the parent or the child asks anything). A reader sees the end of the data only when every twin of every write end is closed, and a process that ends without closing is cleaned by the sweep within about 50 ms. `exec` keeps the process id, so the ends stay and the library hands the table on to the new image ([descriptors](../../libs/descriptors.md)).
* No signal: a write to a broken pipe fails with `EPIPE` and does not kill the writer (signals are stage L3).
* No named pipes, no non-blocking mode, no `select`, and no atomic writes: two writers can interleave chunks of 12 bytes.
* If `pipe` restarts, its ends are lost; the client sees `NOT_CONNECTED`.
