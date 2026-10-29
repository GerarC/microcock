# head

Prints the first lines of files. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
head [-n lines] [path...]
cat path | head -n 3
```

Ten lines by default. With two or more files each one is preceded by `==> <path> <==`. With no path it reads the standard input, and it stops reading as soon as it has the lines. Exit status 0 on success, 1 when a file or the input failed (the others are still printed), 2 for a malformed `-n` (it prints `usage: head [-n lines] [path...]`).

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/head`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/head/command.hpp` | `run`: the loop over the paths and the banners |
| `src/options.cpp`, `include/head/options.hpp` | `parseOptions`: `-n` and where the paths start |
| `src/window.cpp`, `include/head/window.hpp` | `Window`: counts newlines and says how many bytes of a chunk are inside |
| `Makefile` | `NAME = head`, the `SRCS` and `LIBC_SRCS` lists and `STREAM_CLIENT = yes` |

## Build, run, test

```sh
make userland/apps/head
bash scripts/qemu_userland_test.sh
```

The userland script prints the first line of two files and checks the banners.

## Limits and known gaps

- Only `-n lines`; no `-c`, no `-5` form, no `-q`.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
