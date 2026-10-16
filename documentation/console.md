# Console server

`console` is a Ring 3 program that owns text input and output for the programs that use it. It needs nothing from the kernel that a program cannot have: the serial port is a capability (`io:0x3F8+8` in `initrd/manifest.conf`), the screen belongs to `vga_driver` and the keyboard to `kbd`. The kernel keeps `SYS_WRITE` as the debug and fallback channel (early boot, programs that do not link the client, and the kernel logger).

## Pieces
| Piece | File |
|-------|------|
| Server | `userland/console/main.cpp` |
| Protocol and packing | `shared/include/cock/abi/console.hpp` |
| Line editor (no I/O, tested on the host) | `shared/include/cock/abi/console_line.hpp`, `test/host/test_console.cpp` |
| Client | `userland/lib/include/cock/user/console.hpp` |
| C library hooks | `libc/unistd/console.cpp` (`__console_write`, `__console_read`), used by `write.cpp` and `read.cpp` |
| Test program | `test/userland/conclient` |

## Protocol
Requests are synchronous calls (see [ipc](ipc.md)) with four words. Word 0 holds the operation in its low byte and a length in the next byte; words 1 to 3 hold up to 12 bytes of data, the first byte in the low bits of word 1. The reply has the same layout with the status in place of the operation and a count in place of the length.

| Operation | Request | Reply |
|-----------|---------|-------|
| `WRITE` | length 1 to 12 and the bytes | `OK` and the number of bytes taken |
| `READ` | the most bytes wanted, 1 to 12 | `OK`, the count (0 when no line is ready) and the bytes |

A length of 0 or more than 12 is `BAD_REQUEST`. The client splits longer texts into chunks of 12 bytes.

## What the server does
* Output: each chunk goes to the serial port (waiting for the transmitter to be empty) and to `vga_driver` as one `VIDEO_WRITE_STR` message. The display server is the `peer` of the manifest line.
* Input: `kbd` sends `CONSOLE_KEY` with a character. The server accepts it only from the process that `lookup("kbd")` returns, so a program cannot type for the user. The line discipline echoes printable characters, handles Backspace and, on Enter, moves the whole line plus the newline to a 512 byte ready queue. A line holds at most 128 characters; more are ignored. Nothing is readable before Enter.
* One loop: `ipc_accept` returns a call, or `MESSAGE_PENDING` when a key arrived, and then the server empties its inbox with `ipc_try_recv`.

## The client and the C library
`cock::user::console::Client` looks the server up by name (again if it died), and offers `write` and `read`. `libc/unistd/console.cpp` exports `__console_write` for descriptors 1 and 2 and `__console_read` for descriptor 0. `write` and `read` call them when the program linked that file (the symbols are weak), and fall back to `SYS_WRITE` and `SYS_READ` when it did not or the server is not found. Descriptors 3 and up still go to the file client. `printf` and `putchar` need no change.

A program that wants the console adds `unistd/write.cpp unistd/read.cpp unistd/console.cpp` to `LIBC_SRCS`, as `test/userland/conclient/Makefile` does.

## Limits
* `read(0)` does not block in the kernel. A server thread can answer only the call it is serving, so it cannot hold readers; when no line is ready the library sleeps one tick (10 ms) and asks again. Parking the reader needs a thread per reader or a reply to a call other than the current one.
* Any process may read the keyboard and write the screen: the call has no capability check until stage 9 adds the IPC capability.
* Input is line oriented; there is no raw mode and no Ctrl+C.
* The echo goes to the screen only, not to the serial port.
* Other programs still print through `SYS_WRITE`; the lines they print and the lines of `console` can interleave on the screen.

## Testing
`scripts/qemu_userland_test.sh` types `a b shift-c d ret`. `conclient` prints through the server, reads the line with `read(0)` and prints `conclient: line abCd`; the script requires that text on the serial log and on the screen. The host test covers the line editor and the packing.
