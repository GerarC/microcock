# mkdir

Makes directories. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
mkdir path...
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: mkdir path...`). Failures print `mkdir: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/mkdir` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/mkdir/command.hpp` | `run`: `mkdir` of the C library for each path |
| `Makefile` | `NAME = mkdir`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/mkdir
bash scripts/qemu_userland_test.sh
```

The userland script types `mkdir` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- No `-p`: the parent must exist.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
