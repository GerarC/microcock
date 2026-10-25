# ls

Lists a directory, the working directory by default. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
ls [path]
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: ls [path]`). Failures print `ls: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/ls` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/ls/command.hpp` | `run`: the count of arguments |
| `src/directory.cpp`, `include/ls/directory.hpp` | `list`: open, stat and read the directory through the `vfs` client |
| `src/location.cpp`, `include/ls/location.hpp` | `resolveLocation`: a typed path against `getcwd` |
| `Makefile` | `NAME = ls`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/ls
bash scripts/qemu_userland_test.sh
```

The userland script types `ls` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- Directories end in a slash; for a file it prints the path as typed.
- Only one path; no options yet.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
