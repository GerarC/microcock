# touch

Creates empty files. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
touch path...
```

A file that exists is left as it is: the file system has no call that changes a time without writing, so the modification time of an existing file does not change. Exit status 0 when every file exists afterwards, 1 when one could not be created (the others are still made), 2 for no path (it prints `usage: touch path...`). Failures print `touch: <path>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/touch`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/touch/command.hpp` | `run`: `open` with `O_WRONLY | O_CREAT` and `close` for each path |
| `Makefile` | `NAME = touch`, the `SRCS` and `LIBC_SRCS` lists and `STREAM_CLIENT = yes` |

## Build, run, test

```sh
make userland/apps/touch
bash scripts/qemu_userland_test.sh
```

The userland script creates a file with `touch` and counts it with `wc`.

## Limits and known gaps

- No options and no change of the time of an existing file.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
