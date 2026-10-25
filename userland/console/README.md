# console

The console server. It owns text input and output for the programs that use it: output goes to the serial port and to the display server, input is a line editor fed by the keyboard driver. The kernel keeps `SYS_WRITE` only as the debug and fallback channel.

## Where it sits

```text
sh, other programs --ipc_call--> console --VIDEO_WRITE_*--> vga_driver
                                    ^
kbd ------------CONSOLE_KEY---------+
```

- Capability: `io:0x3F8+8` (the serial port), granted by `rs` in `initrd/manifest.conf`, which also lists `ipc=vga_driver`. The test manifest adds nothing for it.
- `rs` sends `CAPABILITY_GRANTED`. The server waits for that message before it opens the serial port, connects to the display server (`cock::abi::console::DISPLAY_NAME`) through the name server and prints `console: ready`; without a display it prints `console: no display server` and keeps the serial output.
- It asks the name server for `kbd` (`cock::abi::console::KEYBOARD_NAME`) and accepts `CONSOLE_KEY` only from that process, so a program cannot type for the user.
- It opts out of the console client of the C library (`CONSOLE_CLIENT = no`), because it would call itself.

## Protocol

Header: `shared/include/cock/abi/console.hpp`. Requests are synchronous calls (`ipc_call`, four words, see `shared/include/cock/abi/call.hpp`). Word 0 carries the operation in the low byte, a length in the next byte and the `LAST` flag in bit 16; words 1 to 3 carry up to 12 bytes. The reply has the same layout with a status and a count.

| Operation | Request | Reply |
|-----------|---------|-------|
| `WRITE` | 1 to 12 bytes, `LAST` on the final chunk of one `write` | `OK` and the bytes taken |
| `READ` | the most bytes wanted, 1 to 12 | `OK`, the count and the bytes, held until a line is ready (`SYS_IPC_DEFER`, answered later with `SYS_IPC_REPLY_TO`) |
| `CLEAR` | none | `OK`; the display server is told to clear the screen |

Statuses: `OK`, `BAD_REQUEST` (length 0 or above 12, unknown operation), `BUSY` (eight readers already wait). The line editor is `shared/include/cock/abi/console_line.hpp` (`LineDiscipline`, a 128 character line, a 512 byte ready queue, Backspace, echo kinds), free of I/O and tested on the host.

Messages used toward other servers: `OUTPUT_WRITE_STR` (chunks of at most 20 bytes), `OUTPUT_WRITE_CHAR` (the echo of keys and Backspace) and the `NONE` message that clears the screen.

## Behaviour

- Output chunks of each caller are kept in one of 8 slots of 256 bytes and emitted when the chunk marked `LAST` arrives, so one `write` is never split by another program.
- While a slot holds text the loop waits with a 50 ms limit; when it runs out, the server checks the callers with `process_alive` and flushes the text of those that died.
- Input is line oriented: nothing is readable before Enter, and the echo goes to the screen only.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the grant, the serial port, and the `ipc_accept` loop with its timeout |
| `src/grant.cpp`, `include/console/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED` and returns the display server |
| `src/serial.cpp`, `include/console/serial.hpp` | `openSerial` and `serialWrite` for the first serial line |
| `src/display.cpp`, `include/console/display.hpp` | The link to `vga_driver`: `setDisplay`, `screenWrite` in chunks with retries, `screenClear`, `screenCharacter` |
| `src/writers.cpp`, `include/console/writers.hpp` | The slots that hold each writer's text until its last chunk: `handleWrite`, `hasPartialText`, `sweepDeadWriters` |
| `src/input.cpp`, `include/console/input.hpp` | The line discipline, the keyboard check, the echo: `handleMessage`, `drainMessages` |
| `src/readers.cpp`, `include/console/readers.hpp` | Readers held with a deferred reply: `deferRead`, `fillRead`, `wakeReaders` |
| `src/requests.cpp`, `include/console/requests.hpp` | `serve`: write, clear and read calls |
| `src/reply.cpp`, `include/console/reply.hpp` | `ANSWER_WORDS` and `answer`, the header word of an answer |
| `Makefile` | `NAME = console`, `SRCS` under `src/`, `CONSOLE_CLIENT = no` |
| `../lib/include/cock/user/console.hpp` | The client (`connect`, `write`, `read`, `clear`) |
| `../../libc/unistd/console.cpp`, `read.cpp`, `write.cpp` | Hooks that route descriptors 0 to 2 to the server |

## Build, run, test

```sh
make userland/console
make run
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Use `HOST_TOOLCHAIN=1` where there is no cross compiler. The userland script types shell commands through the QEMU monitor and requires `console: ready`, the command output on the serial log and the last lines on the screen. `contruncated` sends text without the closing chunk and the script requires `contruncated: cut`. `test/host/test_console.cpp` covers the line editor and the packing.

## Limits and known gaps

- A process may read the keyboard and write the screen only when it holds an `IPC` capability for the console, which the name server gives when the program's `ipc=` list names `console` (or the console's names it). Inside the console, `CONSOLE_KEY` is checked against the process the name server calls `kbd`.
- No raw mode and no Ctrl+C.
- A reader whose reply fails (its thread died after the line was taken) loses that line.
- Text of a dead program shows up to 50 ms late; a live program that never sends `LAST` keeps its text in the slot.
- A program that does not link the client prints through `SYS_WRITE`, and its lines can interleave with the server output on the screen.

## Documentation

- [Console server](../../documentation/userland/console.md)
- [Keyboard driver](../../documentation/userland/keyboard.md)
- [VGA display server](../../documentation/userland/vga.md)
- [IPC and synchronous calls](../../documentation/core/ipc.md)
