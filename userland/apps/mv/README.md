# mv

Renames or moves a file or directory inside one volume. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
mv from to
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: mv from to`). Failures print `mv: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/mv` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/mv/command.hpp` | `run`: `rename` of the C library |
| `Makefile` | `NAME = mv`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/mv
bash scripts/qemu_userland_test.sh
```

The userland script types `mv` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- Across mounts it prints `different volumes`.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
