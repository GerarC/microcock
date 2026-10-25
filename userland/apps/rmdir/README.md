# rmdir

Removes empty directories. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
rmdir path...
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: rmdir path...`). Failures print `rmdir: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/rmdir` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/rmdir/command.hpp` | `run`: `rmdir` of the C library for each path |
| `Makefile` | `NAME = rmdir`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/rmdir
bash scripts/qemu_userland_test.sh
```

The userland script types `rmdir` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
