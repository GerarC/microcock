# sh

The minimal shell. It prints a prompt, reads a line from the console server, splits it into words and runs a built-in command. It is the first program a person talks to and an ordinary Ring 3 program with no capabilities.

## Where it sits

```text
keyboard --> kbd --> console --read(0)--> sh --FS_*--> vfs --> ramfs, fat, ext
                       ^                   |
                       +-----printf-------+
```

- It is spawned last from the initrd (`sh path=bin/sh` in `initrd/manifest.conf`), after `vfs`, `ext` and `kbd`. It has no grants and no argument, so `rs` sends it nothing.
- Terminal I/O uses the C library: descriptor 0 reads a line from `console` and descriptor 1 prints through it (`libc/unistd/console.cpp`). `clear` calls the console client directly (`cock::user::console::Client`).
- Files go through `cock::user::fs::Client` to `vfs` (default server name `vfs`). Messages that arrive while it waits for a reply are stashed with `stash_message`.
- The version comes from `VERSION_MAJOR`, `VERSION_MINOR` and `VERSION_PATCH` of the root `Makefile`, passed as `SH_VERSION_*`. At boot it prints `microcock v<major>.<minor>.<patch> shell, type help` and the prompt `cock> `.

## Protocol

The shell serves nothing. It uses the console protocol (`shared/include/cock/abi/console.hpp`: `READ` in chunks of 12 bytes, `WRITE`, `CLEAR`) and the file protocol (`shared/include/cock/abi/fs.hpp`) with these flags for `write`: `OPEN_WRITE | OPEN_CREATE | OPEN_TRUNCATE`. Failures print `sh: <command>: <path>: <reason>`, with the reason taken from the file status (`not found`, `read only`, `already exists`, `not empty`, `different volumes`, `no space`, `busy`, and so on).

| Command | Effect |
|---------|--------|
| `help` | List the commands |
| `version` | Print `microcock vMAJOR.MINOR.PATCH` |
| `echo text...` | Print the words joined by single spaces |
| `cd [path]` | Change the current directory (`/` without a path) |
| `pwd` | Print the current directory |
| `ls [path]` | List a directory (the current one by default); directories end in `/`; a file prints its own path |
| `cat path` | Print a file |
| `write path text...` | Create or replace a file with the text and a newline |
| `mkdir path`, `rmdir path` | Make a directory, remove an empty one |
| `rm path` | Delete a file |
| `mv from to` | Rename or move inside one volume |
| `ps` | List the processes of `pm`: `PID PPID STAT` (`R` runs, `X` exited) |
| `sleep seconds` | Wait, up to 3600 seconds |
| `clear` | Clear the screen |

A path may be relative to the current directory, with `.` and `..`. A word that is not a built-in names a program: without a `/` it is `/bin/<name>`, with one it is a path. The shell makes a copy of itself with `fork`, the copy does `exec` and the shell does `wait`. An unknown program prints `sh: <name>: command not found` (status 127), a file that is not a program `sh: <name>: cannot execute` (126), and a status other than 0 prints `sh: <name>: exit status <n>`. More than 8 words print `sh: too many words`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the banner, the prompt and the read and run loop |
| `src/input.cpp`, `include/sh/input.hpp` | `readLine`: one line from descriptor 0, in chunks of 12 bytes |
| `src/commands.cpp`, `include/sh/commands.hpp` | The command table, `help`, `run` (split a line and call a command) and `usage` |
| `src/directory_commands.cpp`, `include/sh/directory_commands.hpp` | `cd` and `pwd`, over `chdir` and `getcwd` of the C library |
| `src/paths.cpp`, `include/sh/paths.hpp` | `resolvePath`: a word of a command against the current directory, for the file commands |
| `src/builtins.cpp`, `include/sh/builtins.hpp` | `version`, `echo`, `sleep` and `clear` (the console client lives here) |
| `src/file_commands.cpp`, `include/sh/file_commands.hpp` | `ls`, `cat`, `write`, `mkdir`, `rmdir`, `rm`, `mv`, the file client and `statusText` |
| `src/process_commands.cpp`, `include/sh/process_commands.hpp` | `ps` |
| `src/manager.cpp`, `include/sh/manager.hpp` | The connection to `pm`, shared by `ps` and the program runner |
| `src/program_commands.cpp`, `include/sh/program_commands.hpp` | `runProgram` (`fork`, `exec`, `wait`), the status codes and `lastStatus` |
| `include/sh/arguments.hpp` | Positions of the words of a line and the sizes shared by the modules |
| `include/sh/version.hpp` | The version printed by the shell, from the `SH_VERSION_*` flags |
| `Makefile` | `NAME = sh`, the `SRCS` under `src/`, the `LIBC_SRCS` list and the version flags |
| `../../../shared/include/cock/shell/words.hpp` | `split`, `join` and `parseNumber`, with no I/O, tested on the host |
| `../../../test/host/test_shell.cpp` | Host tests of the line splitter and number parser |

## Build, run, test

```sh
make userland/apps/sh
make disk
make run DISK=build/disk.img
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Add `HOST_TOOLCHAIN=1` without the cross compiler. Type in the QEMU window; without `DISK` only `/` (the initrd, read only) exists. The userland script types its commands through the QEMU monitor (`echo`, `version`, `ps`, `ls`, `cat`, `write`, `mv`, `rm`, a failing `cat`, `cd` and `pwd` with relative paths, `..` and two failing `cd`, `sleep`, `clear` and a last `echo`) and requires their output on the serial log, the last prompt on the screen and a clean `e2fsck` afterwards.

## Limits and known gaps

- The commands of the table are still built in; they become programs of `/bin` one at a time. No `PATH` and no `$?` yet.
- No quoting, pipes, redirection, history, tab completion or Ctrl+C.
- `..` is resolved on the text of the path, so a link in the path is not followed first.
- A line holds at most 127 characters; `write` and `echo` text is at most 255 characters.
- `/` is the initrd and read only, so `write`, `mkdir`, `rm` and `mv` need a disk under `/fat` or `/ext`.
- `cat` writes bytes as they are, so a binary file prints garbage.
- Each `ls` and `cat` opens and closes its own handle; there is no cache.

## Documentation

- [The shell](../../../documentation/userland/shell.md)
- [Console server](../../../documentation/userland/console.md)
- [File system](../../../documentation/userland/filesystem.md)
- [Userland programs and the manifest](../../../documentation/userland/userland.md)
