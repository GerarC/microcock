# wc

Counts lines, words and bytes. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
wc [path...]
echo text | wc
```

Prints `<lines> <words> <bytes> <path>` for each file and a `total` line when there are two or more; with no path it counts the standard input and prints no name. A line is a newline character, a word is a run of characters that are not white space. Exit status 0 on success, 1 when a file or the input failed (the others are still counted). Failures print `wc: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/wc`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/wc/command.hpp` | `run`: the loop over the paths, the print and the total |
| `src/counts.cpp`, `include/wc/counts.hpp` | `Counts`: the three counters and the word that continues across chunks |
| `Makefile` | `NAME = wc`, the `SRCS` and `LIBC_SRCS` lists and `STREAM_CLIENT = yes` |

## Build, run, test

```sh
make userland/apps/wc
bash scripts/qemu_userland_test.sh
```

The userland script counts an empty file, a two line file and the end of a pipe.

## Limits and known gaps

- Counters are 32 bit; no options (`-l`, `-w`, `-c`); characters are bytes.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
