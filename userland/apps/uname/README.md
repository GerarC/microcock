# uname

Prints `microcock v<major>.<minor>.<patch>`. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
uname
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: uname`). Failures print `uname: <subject>: <reason>`.

## Where it sits

It needs the console only. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/uname` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/uname/command.hpp` | `run`: prints the version |
| `include/uname/version.hpp` | `VERSION_MAJOR`, `VERSION_MINOR` and `VERSION_PATCH`, from the `UNAME_VERSION_*` flags the `Makefile` passes |
| `Makefile` | `NAME = uname`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/uname
bash scripts/qemu_userland_test.sh
```

The userland script types `uname` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- The version comes from the root `Makefile` (`VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_PATCH`); a build without them prints `v0.0.0`.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
