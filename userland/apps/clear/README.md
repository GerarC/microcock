# clear

Clears the screen. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
clear
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: clear`). Failures print `clear: <subject>: <reason>`.

## Where it sits

It needs the `console` server (`CLEAR`). Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/clear` and nobody starts it but the shell. It clears the terminal it inherited, not always the first one (the C library keeps the terminal and `__console_clear` uses it).

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/clear/command.hpp` | `run`: asks the console server to clear |
| `Makefile` | `NAME = clear`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/clear
bash scripts/qemu_userland_test.sh
```

The userland script types `clear` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
