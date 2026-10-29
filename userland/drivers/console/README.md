# console

The console driver. It owns the output of the programs that use it: text goes to the serial port and to the display server. It has no line logic: the typed line, the readers and the history belong to the [`tty` server](../../servers/tty/README.md), which asks this driver to draw the echo. The kernel keeps `SYS_WRITE` only as the debug and fallback channel.

## Where it sits

```text
sh, other programs --WRITE, CLEAR--> console --VIDEO_WRITE_*--> vga_driver
tty ---------------------ECHO---------^
```

- Capability: `io:0x3F8+8` (the serial port), granted by `rs` in `initrd/etc/manifest.conf`, which also lists `ipc=vga_driver`. The test manifest adds nothing for it.
- `rs` sends `CAPABILITY_GRANTED`. The driver waits for that message before it opens the serial port, connects to the display server (`cock::abi::console::DISPLAY_NAME`) through the name server and prints `console: ready`; without a display it prints `console: no display server` and keeps the serial output.
- It asks the name server for `tty` (`cock::abi::console::TTY_NAME`) and accepts `ECHO` only from that process, so a program cannot draw as if it were typing.
- It opts out of the console client of the C library (`CONSOLE_CLIENT = no`), because it would call itself.

## Protocol

Header: `shared/include/cock/abi/console.hpp`. Requests are synchronous calls (`ipc_call`, four words, see `shared/include/cock/abi/call.hpp`). Word 0 carries the operation in the low byte, a length in the next byte and the `LAST` flag in bit 16; words 1 to 3 carry up to 12 bytes. The reply has the same layout with a status and a count.

| Operation | Request | Reply |
|-----------|---------|-------|
| `WRITE` | 1 to 12 bytes, `LAST` on the final chunk of one `write` | `OK` and the bytes taken |
| `CLEAR` | none | `OK`; the display server is told to clear the screen |
| `ECHO` | 1 to 12 bytes of the typed line, a backspace erases | `OK` and the bytes drawn; `DENIED` unless the caller is `tty`; screen only |
| `SHOW` | none | `OK`; the display server puts the terminal of the header on the screen; `DENIED` unless the caller is `tty` |

Bits 24 to 31 of the header name the terminal; a number out of range is `BAD_REQUEST`. The driver sends `OUTPUT_SELECT_TERMINAL` to `vga_driver` whenever the terminal of the text changes.

Statuses: `OK`, `BAD_REQUEST` (length 0 or above 12, unknown operation, terminal out of range, and `READ` and `ATTACH`, which `tty` serves), `DENIED`. Messages used toward other servers: `OUTPUT_WRITE_STR` (chunks of at most 20 bytes), `OUTPUT_WRITE_CHAR` (the echo and Backspace) and the `NONE` message that clears the screen.

## Behaviour

- Output chunks of each caller are kept in one of 8 slots of 256 bytes and emitted when the chunk marked `LAST` arrives, so one `write` is never split by another program.
- While a slot holds text the loop waits with a 50 ms limit; when it runs out, the driver checks the callers with `process_alive` and flushes the text of those that died.
- Messages that arrive asynchronously are dropped: nothing sends the driver any.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the grant, the serial port, and the `ipc_accept` loop with its timeout |
| `src/grant.cpp`, `include/console/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED` |
| `src/serial.cpp`, `include/console/serial.hpp` | `openSerial` and `serialWrite` for the first serial line; `serialWrite` reads and drops the color sequences (one reader for each terminal) |
| `src/display.cpp`, `include/console/display.hpp` | The link to `vga_driver`: `connectDisplay`, `screenWrite` in chunks with retries, `screenClear`, `screenCharacter`, `screenShow`; each names the terminal |
| `src/writers.cpp`, `include/console/writers.hpp` | The slots that hold each writer's text until its last chunk: `handleWrite`, `hasPartialText`, `sweepDeadWriters` |
| `src/tty_link.cpp`, `include/console/tty_link.hpp` | `isTty`: whether a caller is the `tty` server |
| `src/requests.cpp`, `include/console/requests.hpp` | `serve`: write, clear and echo calls |
| `src/reply.cpp`, `include/console/reply.hpp` | `ANSWER_WORDS` and `answer`, the header word of an answer |
| `Makefile` | `NAME = console`, `SRCS` under `src/`, `CONSOLE_CLIENT = no` |
| `../../lib/include/cock/user/console.hpp` | The client (`connect`, `write`, `clear`, `echo`, `read` through `tty`) |
| `../../../libc/unistd/console.cpp`, `read.cpp`, `write.cpp` | Hooks that route descriptors 0 to 2 to the servers |

## Build, run, test

```sh
make userland/drivers/console
make run
bash scripts/qemu_userland_test.sh
```

Use `HOST_TOOLCHAIN=1` where there is no cross compiler. The userland script types shell commands through the QEMU monitor and requires `console: ready`, `tty: ready`, the command output on the serial log and the last lines on the screen. `contruncated` sends text without the closing chunk and the script requires `contruncated: cut`.

## Limits and known gaps

- A process may write the screen only when it holds an `IPC` capability for the console, which the name server gives when the program's `ipc=` list names `console`.
- Text of a dead program shows up to 50 ms late; a live program that never sends `LAST` keeps its text in the slot.
- A program that does not link the client prints through `SYS_WRITE`, and its lines can interleave with the driver output on the screen.

## Documentation

- [Console driver](../../../documentation/userland/console.md)
- [The tty server](../../../documentation/userland/tty.md)
- [Keyboard driver](../../../documentation/userland/keyboard.md)
- [VGA display server](../../../documentation/userland/vga.md)
- [IPC and synchronous calls](../../../documentation/core/ipc.md)
