# Console driver

`console` is a Ring 3 driver that owns the output of the programs that use it: the serial port and, through `vga_driver`, the screen. It needs nothing from the kernel that a program cannot have: the serial port is a capability (`io:0x3F8+8` in `initrd/etc/manifest.conf`) and the screen belongs to `vga_driver`. The kernel keeps `SYS_WRITE` as the debug and fallback channel (early boot, the programs that start before the driver, and the kernel logger). Typed input is not here: it belongs to the [tty server](tty.md), which draws its echo through this driver.

## Pieces
| Piece | File |
|-------|------|
| Driver | `userland/drivers/console/src/` and `userland/drivers/console/include/console/` |
| Protocol and packing | `shared/include/cock/abi/console.hpp` (shared with the `tty` server) |
| Client | `userland/lib/include/cock/user/console.hpp` (writes go here, reads go to `tty`) |
| C library hooks | `libc/unistd/console.cpp` (`__console_write`, `__console_read`), called by the console description of `libc/unistd/descriptor.cpp` |
| First client | `userland/apps/sh` (the shell) |

## Protocol
Requests are synchronous calls (see [ipc](../core/ipc.md)) with four words. Word 0 holds the operation in its low byte, a length in the next byte and a `LAST` flag in bit 16; words 1 to 3 hold up to 12 bytes of data, the first byte in the low bits of word 1. The reply has the same layout with the status in place of the operation and a count in place of the length.

| Operation | Request | Reply |
|-----------|---------|-------|
| `WRITE` | length 1 to 12, the bytes, and `LAST` on the final chunk of one `write` call | `OK` and the number of bytes taken |
| `CLEAR` | nothing | `OK`; the driver sends the display server the message that clears the screen and puts the cursor at the top |
| `ECHO` | length 1 to 12, the bytes of the typed line (a backspace erases) | `OK` and the number of bytes drawn; `DENIED` unless the caller is the `tty` server. Screen only, not the serial port |
| `SHOW` | nothing; the terminal is in the header | `OK`; the display server puts that terminal on the screen. `DENIED` unless the caller is the `tty` server |

Bits 24 to 31 of the header name the terminal the call is about (see [tty](tty.md#terminals)); a number out of range, a length of 0 or more than 12, `READ` and `ATTACH` (which the `tty` server serves) are `BAD_REQUEST`. The client splits longer texts into chunks of 12 bytes.

## What the driver does
* **Output.** A `write` of a program arrives as several chunks. The driver keeps the chunks of each caller in a slot (8 slots of 256 bytes) and emits them (on the terminal of the last chunk) when the chunk marked `LAST` arrives, so the text of one `write` is never split by the output of another program. The text goes to the serial port (waiting for the transmitter to be empty) and to `vga_driver` as `OUTPUT_WRITE_STR` messages of at most 20 bytes. A caller that finds no free slot is written at once; a slot of a dead caller is flushed and reused. While any slot holds text, the driver waits for work with a 50 ms limit; when the limit runs out it checks the callers of the slots with `process_alive` and flushes the text of those that died, so a program killed in the middle of a `write` still shows what it had sent. The display server is the `peer` of the manifest line.
* **Echo.** `ECHO` draws characters on the screen with `OUTPUT_WRITE_CHAR`. Before it sends any text the driver tells `vga_driver` which terminal it is for (`OUTPUT_SELECT_TERMINAL`, only when it changed). The driver asks the name server who `tty` is and refuses anyone else, so a program cannot draw as if it were typing. There is no line logic in the driver.
* **One loop.** `ipc_accept` returns a call, or `MESSAGE_PENDING` when a message arrived; the driver drops those messages, since nothing sends it any.

## The client and the C library
`cock::user::console::Client` connects to the driver through the name server (again if it died; the manifest of the program must list `console` in `ipc=`), and offers `write`, `clear` and `echo`. `read` connects to the `tty` server the first time (the manifest must list `tty` too). `libc/unistd/console.cpp` exports `__console_write` for output and `__console_read` for input. The table of descriptors (`libc/unistd/descriptor.cpp`, see [descriptors](../libs/descriptors.md)) starts with descriptors 0, 1 and 2 as console descriptions that call them, and falls back to `SYS_WRITE` and `SYS_READ` when the server is not found; a program that opts out of the console client has no table and `write` and `read` call the system calls directly. Because the console is a description like any other, `dup2` can point descriptor 1 at a file. `printf` and `putchar` need no change.

`userland/common.mk` links the client into every program (`unistd/read.cpp` and `unistd/console.cpp` are added to `LIBC_SRCS`). A program opts out with `CONSOLE_CLIENT = no` before the include. These do, and for a reason:

| Program | Why it opts out |
|---------|-----------------|
| `console` | It would call itself |
| `vga_driver` | The server draws on it |
| `kbd` | The input path does not depend on the server it feeds |
| `rs` | It prints before the driver has its capabilities, and a call to a server that waits for its grant would block both |
| `ramfs` | It starts before the driver |

## Limits
* Only a process that holds a handle on the endpoint of `console` (which the name server gives when the manifest allows it) can call it; the shipped manifest gives it to `sh`, `tty`, `vfs`, `fat`, `ext`, `ata`, `rtc`, `init` and `pipe`.
* The echo goes to the screen only, not to the serial port.
* A program that does not link the client still prints through `SYS_WRITE`, and its lines can interleave with those of `console` on the screen.
* Text of a dead program shows up up to 50 ms late, and a program that stays alive but never sends the `LAST` chunk keeps its text in the slot.

## Testing
`scripts/qemu_userland_test.sh` types commands for the shell, which prints through the driver and reads through `tty`; the script requires `console: ready` and `tty: ready`, their output on the serial log and the last lines on the screen. `contruncated` sends two chunks without `LAST` and exits; the script requires `contruncated: cut` on the serial log.
