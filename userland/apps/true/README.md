# true

Does nothing and ends with exit status 0. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`. It is there to try `$?` and the exit status handling of the shell.

## Use

```text
true
```

Arguments are ignored. The namespace is `cock::true_command` because `true` is a keyword.

## Where it sits

It needs nothing: no console, no capabilities and no manifest line. It is packed into the initrd under `bin/true` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/true/command.hpp` | `run`: returns `STATUS_OK` |
| `Makefile` | `NAME = true`, the `SRCS` and `LIBC_SRCS` lists |

## Build, run, test

```sh
make userland/apps/true
bash scripts/qemu_userland_test.sh
```

The userland script types `true;echo t=$?` and checks the serial log.

## Limits and known gaps

- No options.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
