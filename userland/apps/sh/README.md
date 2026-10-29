# sh

The minimal shell. It prints a prompt, reads a line from the console server, splits it into words and runs a built-in command or a program of `/bin`. It is the first program a person talks to and an ordinary Ring 3 program with no capabilities.

## Where it sits

```text
keyboard --> kbd --> console --read(0)--> sh --FS_*--> vfs --> ramfs, fat, ext
                       ^                   |
                       +-----printf-------+
```

- It is spawned last from the initrd (`sh path=bin/sh` in `initrd/etc/manifest.conf`), after `vfs`, `ext` and `kbd`. It has no grants and no argument, so `rs` sends it nothing.
- Terminal I/O uses the C library: descriptor 0 reads a line from `console` and descriptor 1 prints through it (`libc/unistd/console.cpp`). - The shell reaches the file server only through `chdir` (the C library). The programs of `/bin` do the file work.
- The version comes from `VERSION_MAJOR`, `VERSION_MINOR` and `VERSION_PATCH` of the root `Makefile`, passed as `SH_VERSION_*`. At boot it prints `microcock v<major>.<minor>.<patch> shell, type help` and the prompt `cock:<directory>> ` (`sh/prompt.hpp`), with the name in bright green and the directory in bright cyan through ANSI color sequences.

## Protocol

The shell serves nothing. It uses the console protocol (`shared/include/cock/abi/console.hpp`: `READ` in chunks of 12 bytes, `WRITE`), `pm` (`fork`, `exec`, `wait`) and the pipe server (`pipe`). `cd` prints `sh: cd: <path>: <reason>`.

| Command | Effect |
|---------|--------|
| `help` | List the built-in commands and the programs of `/bin` |
| `cd [path]` | Change the current directory (`/` without a path) |
| `pwd` | Print the current directory |
| `exit [status]` | End the shell with the status, or with the status of the last program |

The other commands are programs of `/bin`, each in `userland/apps/<name>`: `ls`, `cat`, `mkdir`, `rmdir`, `rm`, `mv`, `echo`, `ps`, `sleep`, `clear` `uname`, `true`, `false` and `date`.

A path may be relative to the current directory, with `.` and `..`. A word that is not a built-in names a program: without a `/` it is `/bin/<name>`, with one it is a path. The shell makes a copy of itself with `fork`, the copy does `exec` and the shell does `wait`. An unknown program prints `sh: <name>: command not found` (status 127), a file that is not a program `sh: <name>: cannot execute` (126), and a status other than 0 prints `sh: <name>: exit status <n>`. More than 16 words print `sh: too many words`. Single and double quotes and a backslash keep spaces inside a word (`echo 'a  b'`), and a quote never closed prints `sh: unterminated quote`. A program that fails prints its own message (`cat: /x: not found`) and ends with 1, so the shell adds `sh: cat: exit status 1`.

Redirection and pipes: `cmd > file` creates or truncates the file and makes it the output of the program, `cmd >> file` appends, `cmd < file` reads the input from it, and `a | b | c` connects the output of each program to the input of the next, up to four programs. The operators need no spaces (`echo hi>f`) and lose their meaning between quotes or after a backslash. A built-in command cannot be redirected or piped; see `documentation/userland/shell.md`.

Variables: `$?` is the status of the last program, `$PWD` the current directory, and `NAME=value` sets a variable that `$NAME` gives back (`set` lists them, `unset NAME` removes one). They are expanded before the line is split and are not passed to programs; see `documentation/userland/shell.md`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the banner, the prompt and the read and run loop |
| `src/input.cpp`, `include/sh/input.hpp` | `readLine`: one line from descriptor 0, in chunks of 12 bytes |
| `src/commands.cpp`, `include/sh/commands.hpp` | The command table, `help`, `run` (split a line and call a command) and `usage` |
| `src/directory_commands.cpp`, `include/sh/directory_commands.hpp` | `cd` and `pwd`, over `chdir` and `getcwd` of the C library |
| `src/exit_command.cpp`, `include/sh/exit_command.hpp` | `exit` |
| `src/paths.cpp`, `include/sh/paths.hpp` | `resolvePath`: a program path typed with a `/` against the current directory |
| `src/manager.cpp`, `include/sh/manager.hpp` | The connection to `pm`, for the program runner |
| `src/program_commands.cpp`, `include/sh/program_commands.hpp` | `runPipeline` (the pipes, `fork`, the descriptors of each copy, `exec`, `wait`), the status codes and `lastStatus` |
| `include/sh/arguments.hpp` | Positions of the words of a line and the sizes shared by the modules |
| `include/sh/version.hpp` | The version printed in the banner, from the `SH_VERSION_*` flags |
| `Makefile` | `NAME = sh`, the `SRCS` under `src/`, the `LIBC_SRCS` list (with the pipe client) and the version flags |
| `../../../shared/include/cock/shell/words.hpp` | `split` (words and the operators `|`, `<`, `>`, `>>`), `join` and `parseNumber`, with no I/O, tested on the host |
| `../../../shared/include/cock/shell/pipeline.hpp` | `parsePipeline`: the tokens grouped into commands with their redirections, tested on the host |
| `../../../test/host/test_shell.cpp` | Host tests of the line splitter, the pipeline parser and the number parser |

## Build, run, test

```sh
make userland/apps/sh
make disk
make run DISK=build/disk.img
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Add `HOST_TOOLCHAIN=1` without the cross compiler. Type in the QEMU window; without `DISK` only `/` (the initrd, read only) exists. The userland script types its commands through the QEMU monitor (`echo`, `uname`, `ps`, `ls`, `cat`, `echo text > file`, `mv`, `rm`, a failing `cat`, `mkdir` and `rmdir`, `cd` and `pwd` with relative paths, `..` and two failing `cd`, `sleep`, `clear` and a last `echo`, plus redirection with `>`, `>>` and `<`, pipes of two and three programs and the refused lines) and requires their output on the serial log, the last prompt on the screen and a clean `e2fsck` afterwards.

## Limits and known gaps

- Only `help`, `cd`, `pwd`, `set`, `unset` and `exit` are built in; there is no `PATH`.
- No history, tab completion or Ctrl+C. A pipeline has at most 4 commands. Redirection is only for programs (not for `cd`, `pwd`, `exit` and `help`), only `<`, `>` and `>>` on descriptors 0 and 1 (no `2>`), and only on the first (input) and last (output) command.
- `..` is resolved on the text of the path, so a link in the path is not followed first.
- A line holds at most 127 characters.
- `/` is the initrd and read only, so `>`, `mkdir`, `rm` and `mv` need a disk under `/fat` or `/ext`.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Console server](../../../documentation/userland/drivers/console.md)
- [File system](../../../documentation/userland/servers/fs/filesystem.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
