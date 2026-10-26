# tty

The terminal server. It owns the typed input: the line being edited, the finished lines and the programs that wait for them. It holds no capability. It takes keys from `kbd`, asks the [console driver](../../drivers/console/README.md) to draw their echo, and answers `READ` from programs.

## Where it sits

```text
kbd --CONSOLE_KEY--> tty --ECHO--> console --> vga_driver
                      ^
sh --READ-------------+
```

- Manifest line: `tty path=servers/tty restart=3 ipc=console`, right after `console`. `kbd` lists `ipc=tty`; a program that reads lists `tty` (the shell does). No `caps=` and no grant, so `rs` sends it nothing.
- It asks the name server for `kbd` and accepts `CONSOLE_KEY` only from that process.
- It links the console client of the C library to print `tty: ready` and to draw the echo.

## Protocol

The same four word calls as the driver (`shared/include/cock/abi/console.hpp`). Only `READ` is served: 1 to 12 bytes wanted, the reply carries the count and the bytes and is held until a line is ready (`SYS_IPC_DEFER`, answered later with `SYS_IPC_REPLY_TO`). `BAD_REQUEST` for any other operation or a bad length, `BUSY` when eight readers already wait.

## Behaviour

- The line editor is `shared/include/cock/abi/console_line.hpp` (`LineDiscipline`, a 128 character line, a 512 byte ready queue, Backspace, echo kinds), free of I/O and tested on the host.
- Each echo is one `ECHO` call on the driver; a driver that does not answer loses the echo, not the input.
- When the reply to a reader fails because the reader died, its bytes are kept and the next reader gets them first.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the `ipc_accept` loop |
| `src/input.cpp`, `include/tty/input.hpp` | The line discipline, the keyboard check: `handleMessage`, `drainMessages` |
| `src/output.cpp`, `include/tty/output.hpp` | `echo`: the echo of a key, through the console driver |
| `src/readers.cpp`, `include/tty/readers.hpp` | Readers held with a deferred reply: `inputReady`, `deferRead`, `fillRead`, `wakeReaders`, and the bytes kept for the next reader |
| `src/requests.cpp`, `include/tty/requests.hpp` | `serve`: the read call |
| `src/reply.cpp`, `include/tty/reply.hpp` | `ANSWER_WORDS` and `answer` |
| `Makefile` | `NAME = tty` and the `SRCS` |

## Build, run, test

```sh
make userland/servers/tty
bash scripts/qemu_userland_test.sh
```

Use `HOST_TOOLCHAIN=1` where there is no cross compiler. The userland script types shell commands through the QEMU monitor, so every command crosses `kbd`, `tty`, `console` and `vga_driver`; it requires `tty: ready`. `test/host/test_console.cpp` covers the line editor.

## Limits and known gaps

- Input is line oriented: no raw mode, no history, no Ctrl+C. They go here (stages W9 and W10).
- A restarted `tty` loses the line being edited and the readers that waited.

## Documentation

- [The tty server](../../../documentation/userland/tty.md)
- [Console driver](../../../documentation/userland/console.md)
