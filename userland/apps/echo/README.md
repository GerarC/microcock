# echo

Prints its arguments joined by single spaces, then a newline. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
echo [text...]
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: echo [text...]`). Failures print `echo: <subject>: <reason>`.

## Where it sits

It needs the console only. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/echo` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/echo/command.hpp` | `run`: the loop over the arguments |
| `Makefile` | `NAME = echo`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/echo
bash scripts/qemu_userland_test.sh
```

The userland script types `echo` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- Words with spaces need the quoting of W6; until then a word is what the shell splits on blanks.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
