# false

Does nothing and ends with exit status 1. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`. It is there to try `$?` and the exit status handling of the shell. The shell prints `sh: false: exit status 1` for it, like for any program that fails.

## Use

```text
false
```

Arguments are ignored. The namespace is `cock::false_command` because `false` is a keyword.

## Where it sits

It needs nothing: no console, no capabilities and no manifest line. It is packed into the initrd under `bin/false` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/false/command.hpp` | `run`: returns `STATUS_FAILURE` |
| `Makefile` | `NAME = false`, the `SRCS` and `LIBC_SRCS` lists |

## Build, run, test

```sh
make userland/apps/false
bash scripts/qemu_userland_test.sh
```

The userland script types `false;echo f=$?` and checks the serial log.

## Limits and known gaps

- No options.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
