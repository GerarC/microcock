# date

Prints the date and time of the clock as `YYYY-MM-DD HH:MM:SS`, in UTC. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
date
```

Exit status 0 on success, 1 when the clock cannot be read (it prints `date: clock: not available`), 2 for any argument (it prints `usage: date `).

## Where it sits

It asks the `rtc` server for the time (`cock::user::Clock`, the same client the file servers use), so the shell that starts it lists `rtc` in its `ipc=` and the program inherits the right through `exec`. It holds no capability and has no manifest line: it is packed into the initrd under `bin/date` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/date/command.hpp` | `run`: connects to `rtc`, reads the time and prints it |
| `src/format.cpp`, `include/date/format.hpp` | `formatDateTime`: the text of a `DateTime` |
| `Makefile` | `NAME = date`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/date
bash scripts/qemu_userland_test.sh
```

The userland script types `date` at the prompt of the shell and checks the serial log for a line in that format.

## Limits and known gaps

- UTC only: there is no time zone.
- No options and no format string.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
