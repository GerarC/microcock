# cp

Copies a file. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
cp from to
```

The destination is created when it does not exist and emptied when it does. Exit status 0 on success, 1 when it tried and failed, 2 for the wrong count of arguments (it prints `usage: cp from to`). Failures print `cp: <path>: <reason>` naming the file that failed.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/cp`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/cp/command.hpp` | `run`: checks the two arguments |
| `src/copy.cpp`, `include/cp/copy.hpp` | `copyFile`: opens both files and copies with the reading loop of `cock/user/stream.hpp` |
| `Makefile` | `NAME = cp`, the `SRCS` and `LIBC_SRCS` lists and `STREAM_CLIENT = yes` |
| `../../lib/include/cock/user/stream.hpp` | The reading loop and `writeAll` that `cp`, `wc`, `head`, `grep` and `hexdump` share |

## Build, run, test

```sh
make userland/apps/cp
bash scripts/qemu_userland_test.sh
```

The userland script copies a file and checks the copy with `wc`, `head`, `grep` and `hexdump`.

## Limits and known gaps

- One file at a time, no directories, no options.
- A file copied onto itself fails, because the file server lets only one writer open a file and the source is opened first.
- A destination that fails half way is left with what was written.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
