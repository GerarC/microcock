# sleep

Waits a whole number of seconds, up to 3600. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
sleep seconds
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: sleep seconds`). Failures print `sleep: <subject>: <reason>`.

## Where it sits

It needs the kernel timer (`SYS_SLEEP`). Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/sleep` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/sleep/command.hpp` | `run`: parses the number and calls `sleep_ms` |
| `Makefile` | `NAME = sleep`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/sleep
bash scripts/qemu_userland_test.sh
```

The userland script types `sleep` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- Exit status 2 and `usage: sleep seconds` for a missing, bad or too big number.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
