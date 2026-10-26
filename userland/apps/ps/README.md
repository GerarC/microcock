# ps

Lists the processes `pm` knows, as `PID PPID STAT COMMAND`, in color. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
ps
```

Exit status 0 on success, 1 when it tried and failed, 2 for the wrong arguments (it prints `usage: ps`). Failures print `ps: <subject>: <reason>`.

## Where it sits

It needs the name server and `pm` (it lists `names` and `pm` in its manifest `ipc=`). Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/ps` and nobody starts it but the shell.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/ps/command.hpp` | `run`: one `SNAPSHOT` request, then the header and one line per row |
| `src/columns.cpp`, `include/ps/columns.hpp` | `printHeader` and `printRow`: the aligned columns |
| `src/line.cpp`, `include/ps/line.hpp` | `Line`: a row built in memory and written with one `write` |
| `include/ps/palette.hpp` | The escape sequences of the colors |
| `src/manager.cpp`, `include/ps/manager.hpp` | `connectManager`: the connection to `pm` through the name server |
| `src/state.cpp`, `include/ps/state.hpp` | `stateLetter` (`R`, `S`, `T`, `Z`, `X`, `D` or `?`) and `stateColor` |
| `Makefile` | `NAME = ps`, the `SRCS` and `LIBC_SRCS` lists |
| `../../lib/src/tool.cpp`, `../../lib/include/cock/user/tool.hpp` | The messages and exit statuses every program of `/bin` shares |

## Build, run, test

```sh
make userland/apps/ps
bash scripts/qemu_userland_test.sh
```

The userland script types `ps` at the prompt of the shell and checks the serial log.

## Limits and known gaps

- A process with no parent prints `-` as its parent, and one with no name prints `-` as its name.
- The colors are escape sequences: the screen shows them and the serial line drops them.
- No options; arguments are plain words.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
