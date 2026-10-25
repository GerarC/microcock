# cat

Prints files, byte for byte. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
cat path...
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: cat path...`). Failures print `cat: <subject>: <reason>`.

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/cat` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/cat/command.hpp` | `run`: the loop over the paths |
| `src/copy.cpp`, `include/cat/copy.hpp` | `copyFile`: `open`, `read` in chunks of 512 bytes and `write` to descriptor 1 |
| `Makefile` | `NAME = cat`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/cat
bash scripts/qemu_userland_test.sh
```

The userland script types `cat` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- It uses the C library calls, so a path is resolved against the working directory the program inherited.
- A binary file prints garbage; a file that fails does not stop the next ones.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
