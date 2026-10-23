# Console server

`console` is a Ring 3 program that owns text input and output for the programs that use it. It needs nothing from the kernel that a program cannot have: the serial port is a capability (`io:0x3F8+8` in `initrd/manifest.conf`), the screen belongs to `vga_driver` and the keyboard to `kbd`. The kernel keeps `SYS_WRITE` as the debug and fallback channel (early boot, the programs that start before the server, and the kernel logger).

## Pieces
| Piece | File |
|-------|------|
| Server | `userland/console/src/` and `userland/console/include/console/` |
| Protocol and packing | `shared/include/cock/abi/console.hpp` |
| Line editor (no I/O, tested on the host) | `shared/include/cock/abi/console_line.hpp`, `test/host/test_console.cpp` |
| Client | `userland/lib/include/cock/user/console.hpp` |
| C library hooks | `libc/unistd/console.cpp` (`__console_write`, `__console_read`), used by `write.cpp` and `read.cpp` |
| First client | `userland/sh` (the shell) |

## Protocol
Requests are synchronous calls (see [ipc](../core/ipc.md)) with four words. Word 0 holds the operation in its low byte, a length in the next byte and a `LAST` flag in bit 16; words 1 to 3 hold up to 12 bytes of data, the first byte in the low bits of word 1. The reply has the same layout with the status in place of the operation and a count in place of the length.

| Operation | Request | Reply |
|-----------|---------|-------|
| `WRITE` | length 1 to 12, the bytes, and `LAST` on the final chunk of one `write` call | `OK` and the number of bytes taken |
| `READ` | the most bytes wanted, 1 to 12 | `OK`, the count and the bytes; the reply is held until a line is ready |
| `CLEAR` | nothing | `OK`; the server sends the display server the message that clears the screen and puts the cursor at the top |

A length of 0 or more than 12 is `BAD_REQUEST`. `BUSY` answers a `READ` when the server already holds eight waiting readers; the client sleeps one tick and asks again. The client splits longer texts into chunks of 12 bytes.

## What the server does
* **Output.** A `write` of a program arrives as several chunks. The server keeps the chunks of each caller in a slot (8 slots of 256 bytes) and emits them when the chunk marked `LAST` arrives, so the text of one `write` is never split by the output of another program. The text goes to the serial port (waiting for the transmitter to be empty) and to `vga_driver` as `OUTPUT_WRITE_STR` messages of at most 20 bytes. A caller that finds no free slot is written at once; a slot of a dead caller is flushed and reused. While any slot holds text, the server waits for work with a 50 ms limit; when the limit runs out it checks the callers of the slots with `process_alive` and flushes the text of those that died, so a program killed in the middle of a `write` still shows what it had sent. The display server is the `peer` of the manifest line.
* **Input.** `kbd` sends `CONSOLE_KEY` with a character. The server accepts it only from the process that `lookup("kbd")` returns, so a program cannot type for the user. The line discipline echoes printable characters, handles Backspace and, on Enter, moves the whole line plus the newline to a 512 byte ready queue. A line holds at most 128 characters; more are ignored. Nothing is readable before Enter.
* **Blocked readers.** When `READ` finds no input, the server calls `SYS_IPC_DEFER`, which detaches the call from the server thread and returns a ticket, and keeps the ticket and the length wanted. The server goes back to `ipc_accept`. When Enter makes a line ready, it answers the oldest readers with `SYS_IPC_REPLY_TO`. The reader is blocked in the kernel meanwhile and uses no CPU.
* **One loop.** `ipc_accept` returns a call, or `MESSAGE_PENDING` when a key arrived, and then the server empties its inbox with `ipc_try_recv` and wakes the readers.

## The client and the C library
`cock::user::console::Client` looks the server up by name (again if it died), and offers `write` and `read`. `libc/unistd/console.cpp` exports `__console_write` for descriptors 1 and 2 and `__console_read` for descriptor 0. `write` and `read` call them when the program linked that file (the symbols are weak), and fall back to `SYS_WRITE` and `SYS_READ` when it did not or the server is not found. Descriptors 3 and up still go to the file client. `printf` and `putchar` need no change.

`userland/common.mk` links the client into every program (`unistd/read.cpp` and `unistd/console.cpp` are added to `LIBC_SRCS`). A program opts out with `CONSOLE_CLIENT = no` before the include. These do, and for a reason:

| Program | Why it opts out |
|---------|-----------------|
| `console` | It would call itself |
| `vga_driver` | The server draws on it |
| `kbd` | The input path does not depend on the server it feeds |
| `init` | It prints before the server has its capabilities, and a call to a server that waits for its grant would block both |
| `ramfs` | It starts before the server |

## Limits
* Only a process that holds an IPC capability for `console` (or that `console` holds one for) can call it; the shipped manifest gives it to `sh`, `vfs`, `fat`, `ext`, `ata` and `rtc`. A holder can read the keyboard and write the screen. `CONSOLE_KEY` is still checked by the name `kbd`, which waits for the name service of a later step.
* Input is line oriented; there is no raw mode and no Ctrl+C.
* The echo goes to the screen only, not to the serial port.
* A program that does not link the client still prints through `SYS_WRITE`, and its lines can interleave with those of `console` on the screen.
* A reader whose line was taken and whose `REPLY_TO` failed (its thread died) loses that line. Putting the line back in the queue would fix it; it waits for a consumer that needs it.
* Text of a dead program shows up up to 50 ms late, and a program that stays alive but never sends the `LAST` chunk keeps its text in the slot.

## Testing
`scripts/qemu_userland_test.sh` types commands for the shell, which prints through the server and blocks in `read(0)`; the script requires their output on the serial log and the last lines on the screen. `contruncated` sends two chunks without `LAST` and exits; the script requires `contruncated: cut` on the serial log. The host test covers the line editor and the packing; the `Synchronous IPC` kernel suite and a `probe` check cover `defer` and `reply_to`.
