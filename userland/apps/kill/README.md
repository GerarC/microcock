# kill

Sends a signal to processes. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
kill [-signal] pid...
```

The signal is `TERM` by default, or a name (`INT`, `KILL`, `TERM`, `CONT`, `STOP`) or the number of one of them (2, 9, 15, 18, 19) after a dash: `kill -9 12`, `kill -STOP 12`. A pid is the number of the `PID` column of `ps`. `pm` decides whether the caller may signal the process: the root server may signal any process, anyone else only its descendants.

Exit status 0 when every signal was sent, 1 when one was not (the others are still sent; it prints `kill: <pid>: <reason>` with `not permitted`, `no such process`, `bad request` or `refused by the kernel`), 2 for a missing pid, a pid that is not a number or an unknown signal (it prints `usage: kill [-signal] pid...`, and nothing is sent).

## Where it sits

It talks to `pm` through the name server (`cock::user::names::connect("pm")`), with the `ipc=` right the shell passes through `exec`, as `ps` does. It has no capabilities and no manifest line, because it is packed into the initrd under `bin/kill`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/kill/command.hpp` | `run`: reads the signal and the pids, then sends |
| `src/signal.cpp`, `include/kill/signal.hpp` | `parseSignal`: the table of names and numbers |
| `src/manager.cpp`, `include/kill/manager.hpp` | `connectManager`: the handle to `pm` |
| `src/outcome.cpp`, `include/kill/outcome.hpp` | `describe`: the text of an answer of `pm` |
| `Makefile` | `NAME = kill`, the `SRCS` and `LIBC_SRCS` lists and `STREAM_CLIENT = yes` |

## Build, run, test

```sh
make userland/apps/kill
bash scripts/qemu_userland_test.sh
```

The userland script signals a process that does not exist and sends two malformed calls. A signal that reaches a live process is covered by `sigprobe` through `pm`; a shell cannot yet start a job in the background (W10), so no test types `kill` against a running program.

## Limits and known gaps

- Only the five signals the kernel knows; no `-l`, no `-s`, no `SIG` prefix, no process groups (`kill 0`, negative pids).

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Process manager](../../../documentation/userland/pm.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
