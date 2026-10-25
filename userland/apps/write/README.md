# write

Temporary: replaces a file with a line of text, until the redirection of W8 does the same. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
write path text...
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: write path text...`). Failures print `write: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/write` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/write/command.hpp` | `run`: joins the words and writes them with a newline |
| `Makefile` | `NAME = write`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/write
bash scripts/qemu_userland_test.sh
```

The userland script types `write` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- The line is at most 511 bytes; it goes away with W8.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
