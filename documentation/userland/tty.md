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

## Terminals
There are four terminals (`TERMINAL_COUNT` in `shared/include/cock/abi/console.hpp`), like the virtual consoles of Linux. Alt+F1 to Alt+F4 switch between them; the first is on the screen at boot.
* **Attach.** A shell calls `ATTACH` at its start (`__console_attach`, in `cock/user/console.hpp`). `tty` gives it the lowest terminal that has no living owner, so a shell that `init` started again gets back its own terminal; the terminal of a dead owner is cleared first (its waiting readers and unread lines are dropped).
* **Inherit.** The client keeps the terminal. `fork` copies it with the memory of the process and `exec` carries it in the argument block (`ArgsBlock::terminal`, written by `__console_export` and read by `__console_import` in `crt0`), so a program started from a shell writes and reads on the terminal of that shell. A program that was never attached uses the first terminal.
* **Header.** Bits 24 to 31 of the header of every call name the terminal. The servers refuse a number out of range.
* **Input.** `tty` keeps one line discipline, one set of readers and one owner for each terminal. A key goes to the active terminal only, so what is typed on one never reaches a program of another. A `READ` on a terminal that is not active waits until it is.
* **Output.** `console` tells `vga_driver` which terminal a write is for (`OUTPUT_SELECT_TERMINAL`) before it sends the text, and `vga_driver` keeps one 80x25 image with its cursor for each terminal: text of one terminal never overwrites another. Only the image of the active terminal is mirrored on the hardware; `SHOW` (`OUTPUT_SHOW_TERMINAL`) copies the image of the terminal that is switched to. The serial port gets the text of every terminal, mixed.
* **Switch.** `kbd` sees Alt and F1 to F4 and sends `tty` a `CONSOLE_KEY` with `arg1` set to the terminal plus one; `tty` makes it active and asks the driver to `SHOW` it.

## Manifest
`tty path=servers/tty restart=3 ipc=console` comes right after `console`. `init` starts one shell for each terminal: `sh`, `sh2`, `sh3` and `sh4`, all of `bin/sh`. `kbd` lists `ipc=tty` and a program that reads input lists `tty` in its `ipc=` (the shell does: `ipc=console,tty,vfs,pm`). `tty` has no `caps=` and no grant: `rs` sends it nothing.

## Limits
* Input is line oriented; there is no raw mode, no history and no Ctrl+C yet. They are the next stage (W9, W10): this server is where they go.
* A restarted `tty` loses the lines and the readers of every terminal, and forgets which terminal is active; the screen keeps showing the terminal that was on it.
* There is no command to switch, only the keys. Only the left Alt is read.
* The echo goes to the screen only.
