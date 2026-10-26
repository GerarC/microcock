# The tty server

`tty` (`userland/servers/tty`) is a Ring 3 server that owns the typed input: the line being edited, the finished lines and the programs that wait for them. It holds no capability. It receives the keys of the keyboard driver and asks the [console driver](console.md) to draw their echo; programs read from it through the same protocol as the driver (`shared/include/cock/abi/console.hpp`).

```text
kbd --CONSOLE_KEY--> tty --ECHO--> console --> vga_driver
                      ^
sh --READ-------------+
```

## Pieces
| Piece | File |
|-------|------|
| Server | `userland/servers/tty/src/` and `userland/servers/tty/include/tty/` |
| Line editor (no I/O, tested on the host) | `shared/include/cock/abi/console_line.hpp`, `test/host/test_console.cpp` |
| Protocol | `shared/include/cock/abi/console.hpp` (`TTY_NAME`, `READ`) |
| Client | `cock::user::console::Client::read`, which connects to `tty` the first time a program reads |

## What the server does
* **Keys.** `kbd` sends `CONSOLE_KEY` with a character through a handle on the `tty` endpoint that the name server gave it. The server accepts the key only when the badge of the handle is the process the name server returns for `kbd`, so a program cannot type for the user.
* **Line discipline.** Printable characters are echoed, Backspace erases, and on Enter the whole line plus the newline moves to a 512 byte ready queue. A line holds at most 128 characters; more are ignored. Nothing is readable before Enter.
* **Echo.** Each key gives one `ECHO` call on the console driver (`tty/output.hpp`). A driver that does not answer loses the echo and never the input.
* **Blocked readers.** `READ` with no input detaches the call with `SYS_IPC_DEFER` and keeps the ticket and the length wanted (eight at most; a ninth gets `BUSY` and the client asks again later). When Enter makes a line ready the server answers the oldest readers with `SYS_IPC_REPLY_TO`. If a reply cannot be delivered because the reader died, the bytes are kept and the next reader gets them first, so a finished line is not lost.
* **Other operations.** `WRITE`, `CLEAR` and `ECHO` belong to the driver; `tty` answers `BAD_REQUEST`.

## Manifest
`tty path=servers/tty restart=3 ipc=console` comes right after `console`. `kbd` lists `ipc=tty` and a program that reads input lists `tty` in its `ipc=` (the shell does: `ipc=console,tty,vfs,pm`). `tty` has no `caps=` and no grant: `rs` sends it nothing.

## Limits
* Input is line oriented; there is no raw mode, no history and no Ctrl+C yet. They are the next stage (W9, W10): this server is where they go.
* A restarted `tty` loses the line being edited and the readers that waited.
* The echo goes to the screen only.
