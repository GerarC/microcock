# grep

Prints the lines that contain a text. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
grep text [path...]
cat path | grep text
```

The text is plain characters, not a pattern; an empty text matches every line. With two or more files each line starts with `<path>:`. Exit status 0 when a line matched, 1 when none did (the shell then prints `sh: grep: exit status 1`), 2 for a missing text or when an input could not be read (it wins over a match). Failures print `grep: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/grep`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/grep/command.hpp` | `run`: the loop over the paths and the exit status |
| `src/matcher.cpp`, `include/grep/matcher.hpp` | `LineMatcher`: cuts the chunks into lines, looks for the text and prints the lines that have it |
| `Makefile` | `NAME = grep`, the `SRCS` and `LIBC_SRCS` lists and `STREAM_CLIENT = yes` |

## Build, run, test

```sh
make userland/apps/grep
bash scripts/qemu_userland_test.sh
```

The userland script searches two files, a text that is not there, and the end of a pipe.

## Limits and known gaps

- Only the first 255 bytes of a longer line are compared and printed.
- No patterns, no `-i`, `-v`, `-n`, `-r`.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
