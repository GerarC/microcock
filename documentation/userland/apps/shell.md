# The shell

`sh` (`userland/apps/sh`) is the first program a person talks to. It prints a prompt, reads a line from the [console server](../drivers/console.md), splits it into words and runs a built-in command or a program of `/bin`. It is an ordinary Ring 3 program: it has no capabilities, it reaches files through the `vfs` client (`cock/user/fs.hpp`) and the screen and keyboard through the C library.

## Starting it
`initrd/etc/manifest.conf` starts `sh` last, after `vfs`, `ext` and `kbd`. At boot the screen shows the version and the prompt:
```
microcock v0.2.0 shell, type help
cock:/>
```
The prompt is the name `cock` in bright green, a colon and the current directory in bright cyan (`sh/prompt.hpp`). The colors are ANSI escape sequences (`shared/include/cock/abi/ansi.hpp`): `vga_driver` draws them, with a color kept for each terminal, and `console` drops them from the serial line, so the serial log stays plain text. Any program can print them: `ESC [ 0 m` resets, `1` is bold, `30` to `37` and `90` to `97` choose the foreground and `40` to `47` the background; every other sequence is read and ignored.
Type on the keyboard of the QEMU window. Without a disk only `/` exists (the `initrd`, read only); `make disk` and `make run DISK=build/disk.img` attach a disk so `/fat` and `/ext` can be listed and written (see the README).

## Commands
The shell keeps four built-in commands, because the state they change belongs to the process itself. Every other command is a program of `/bin`, one directory per program in `userland/apps/` (see [Userland programs](../system/programs.md)).

| Command | What it does |
|---------|--------------|
| `help` | Lists the built-in commands and the names of the programs of `/bin` |
| `cd [path]` | Changes the current directory (`/` without a path). It fails with `sh: cd: path: not found`, `not a directory` or `path too long` and leaves the directory as it was |
| `pwd` | Prints the current directory |
| `exit [status]` | Ends the shell with the status given, or with the status of the last program. A bad status prints `usage: exit [status]` |

| Program | What it does |
|---------|--------------|
| `true`, `false` | Do nothing and end with status 0 and 1; they are there to try `$?` |
| `date` | Prints the date and time of the clock as `YYYY-MM-DD HH:MM:SS` (UTC). It asks the `rtc` server, so the shell lists `rtc` in its `ipc=` |
| `uname` | Prints `microcock vMAJOR.MINOR.PATCH` (the numbers come from the `Makefile`) |
| `echo text...` | Prints the words separated by single spaces |
| `ls [path]` | Lists a directory (the current one by default); a name ending in `/` is a directory; a file prints its own path |
| `cat [path...]` | Prints files, one after the other; with no path it copies its standard input (the console, a file with `<` or the end of a pipe) |
| `mkdir path...`, `rmdir path...` | Makes directories, removes empty ones |
| `rm path...` | Deletes files |
| `mv from to` | Renames or moves; both paths must be on the same volume |
| `ps` | Prints the table of the [process manager](../servers/pm.md), one line per process: `PID PPID STAT COMMAND`, in color on the screen (the serial line drops the colors): the state letter is green for `R`, cyan for `S`, yellow for `T`, red for `Z` and gray for `X`, and a process that exited is dimmed. `COMMAND` is the name the process was created with, or the one its `exec` gave it. `STAT` is `R` (a thread is ready or running), `S` (every live thread waits) or `X` (its exit notice arrived); `T`, `Z` and `D` are printed once something produces them, see [process states](../servers/pm.md#process-states). A process with no parent shows `-`. It asks `pm` for the whole table with one `SNAPSHOT` request, which `pm` writes in a shared memory object (it needs `names` and `pm` in the `ipc=` of the shell, which its copy keeps), so the rows agree with each other. A process with no name shows `-` |
| `touch path...` | Creates empty files; an existing file is left as it is (the file system has no call that changes a time without writing) |
| `cp from to` | Copies a file, creating or emptying the destination |
| `wc [path...]` | Prints lines, words and bytes of each file (and a `total`), or of the standard input |
| `head [-n lines] [path...]` | Prints the first lines (10 by default) of each file or of the standard input; two or more files get a `==> path <==` banner |
| `grep text [path...]` | Prints the lines that contain a text (plain characters, not a pattern); with two or more files each line starts with `path:`; exit status 1 when none matched |
| `hexdump [path]` | Prints sixteen bytes a row in hexadecimal with the offset and an ASCII column, and the total size on the last line |
| `kill [-signal] pid...` | Sends `TERM` (or `INT`, `KILL`, `CONT`, `STOP`, by name or number) to the processes through [pm](../servers/pm.md); `pm` decides who may signal whom |
| `sleep seconds` | Waits, up to 3600 seconds |
| `clear` | Clears the screen (the `CLEAR` operation of the console protocol) |

The shell has a current directory, like any process (`libc/unistd/cwd.cpp`; it starts in `/`), and a program started from it inherits the directory. A path that does not start with `/` is taken from it, and `.` and `..` are understood (`..` of `/` is `/`). The programs that use files call the C library (`open`, `unlink`, `mkdir`, `rmdir`, `rename`), which resolves the path with `cock/path/resolve.hpp`, and `ls` resolves it itself, so the servers get absolute paths only. A program prints a failure as `program: path: reason` (`cock/user/tool.hpp`), with the reason taken from `errno`: `not found`, `read only`, `already exists`, `not empty`, `different volumes`, and so on. Its exit status is 1, so the shell adds `sh: program: exit status 1`; a wrong count of arguments prints `usage: program arguments` and ends with 2.

## Running programs
A word that is not a built-in names a program. A name with no `/` is looked for in `/bin` (the `bin` directory of the `initrd`); a name with a `/` is a path, taken from the current directory when it is relative (`./exitwith`). The shell asks `pm` for a copy of itself (`fork`), the copy replaces its image with the file (`cock::user::exec`, with the words of the line as `argc` and `argv`, and the descriptors and the working directory of the shell), and the shell collects the status of the copy with `wait` and keeps it (`lastStatus`, ready for the variable `$?` of a later stage). The program writes on the same console, because the copy inherited the handle of `console` and its descriptors. The code is in `sh/program_commands.hpp` and the connection to `pm` in `sh/manager.hpp`.

| Status | Meaning |
|--------|---------|
| 0 | The program ended well; nothing is printed |
| 127 | There is no such file; the copy prints `sh: name: command not found` |
| 126 | The file exists but is not a loadable program (a directory, an empty file, not an image); the copy prints `sh: name: cannot execute` |
| 125 | The program could not be started: `pm` does not answer or refused the copy; the shell prints `sh: name: cannot start the program` |
| other | The status of the program; the shell prints `sh: name: exit status N` |

While the program runs the shell waits in `wait`, so it reads no line.

## Redirection and pipes
An unquoted `|`, `<`, `>` or `>>` is an operator, with or without spaces around it; quoted or after a backslash it is part of a word, and a variable value never makes one (`expand` escapes them). `cock::shell::split` marks each token as a word or an operator (`TokenKind`) and `cock::shell::parsePipeline` (`shared/include/cock/shell/pipeline.hpp`, host tested) groups them into at most four commands, each with its words, its input file and its output file. A redirection takes the word after it as the file name and can sit anywhere in the command (`echo a > f b` is `echo a b` into `f`); a second one of the same kind replaces the first. Input can be redirected only on the first command and output only on the last, because the middle of a pipeline is already connected. The errors are `sh: missing command`, `sh: missing file name after a redirection`, `sh: too many commands in a pipeline` and `sh: input can only be redirected on the first command and output on the last`.

| Operator | Effect on the command |
|----------|-----------------------|
| `< path` | `open(path, O_RDONLY)` becomes descriptor 0 |
| `> path` | `open(path, O_WRONLY \| O_CREAT \| O_TRUNC)` becomes descriptor 1 |
| `>> path` | `open(path, O_WRONLY \| O_CREAT \| O_APPEND)` becomes descriptor 1 |
| `a \| b` | A pipe: descriptor 1 of `a` is its write end, descriptor 0 of `b` its read end |

The work is `runPipeline` in `sh/program_commands.cpp`. For each command the shell creates the pipe to the next one (`pipe`), asks `pm` for a copy of itself (`fork`) and, in the copy, puts the pipe ends and the files on descriptors 0 and 1 with `dup2` and closes the original numbers, then `exec`s the program, which keeps the descriptors (the table travels in the argument block, see [descriptors](../../libs/descriptors.md)). The shell closes its own copy of each end as soon as the copy exists, so the reader of a pipe sees the end of the data when the writer ends; then it waits for every copy, in order, and the status of the line is the one of the last command. A file that cannot be opened is reported by the copy (`sh: path: reason`, status 1), and the shell then prints `sh: name: exit status 1`. The built-in commands act on the shell itself, so they are refused in a pipeline or with a redirection (`sh: cd: a built-in command cannot be piped or redirected`). The shell reaches the pipe server through the name server, so its line in `initrd/etc/manifest.conf` lists `pipe` in `ipc=`, and its copies inherit that right. A program only keeps a redirected descriptor if it links the file and pipe clients of the C library, which every program of `/bin` does through `STREAM_CLIENT = yes`.

## How a line is read
The console server edits the line (echo and Backspace) and delivers it whole, with its newline, when Enter is pressed. `sh` calls `read(0)` in a loop, 12 bytes at a time, until it sees the newline. The call blocks in the kernel until a line is ready (see [console](../drivers/console.md#what-the-driver-does)). A line holds at most 127 characters and 8 words (`cock/shell/words.hpp`, tested on the host in `test/host/test_shell.cpp`).

## Limits
* Only `help`, `cd`, `pwd` and `exit` are built in. There is no `PATH`, no variable `$?` yet, and a program cannot be run in the background.
* Up and Down recall the last 16 lines (the history belongs to the `tty` terminal, see [tty](../servers/tty.md)). No tab completion or Ctrl+C. No `2>` and no `&>`: only descriptors 0 and 1 are redirected.
* Links are not looked at when `..` is resolved: `cd link/..` goes to the parent of the link's name, not of its target.
* `/` is the `initrd` and is read only, so `>`, `mkdir`, `rm` and `mv` only work under `/fat` and `/ext` and need a disk. A writable `/tmp` in memory is noted in the roadmap (`tmpfs`).
* A line has at most 16 words (the arguments `exec` accepts) and 128 bytes. Quoting: single quotes keep their content literally; double quotes keep it too, except that `\"` and `\\` stand for `"` and `\`; outside quotes `\` makes the next character literal. Quotes join with the text next to them (`a"b c"d` is one word) and `''` is an empty word. A quote that is never closed prints `sh: unterminated quote` and runs nothing. The splitting is `cock::shell::split` (`shared/include/cock/shell/words.hpp`, host tested), done in place.
* Variables (`$?`, `$PWD` and the ones the user sets). Before the split, `cock::shell::expand` (`shared/include/cock/shell/expand.hpp`, host tested) replaces `$?` (status of the last program), `$PWD` (current directory) and `$NAME` (letters, digits and underscore, not starting with a digit) with their values; an unset name gives an empty text, a `$` followed by anything else stays. Nothing is expanded between single quotes and `\$` is a literal `$`. The value is inserted escaped, so it stays one piece of the word and is never split or read as a quote again (`echo "$X"` and `echo $X` print the same). A line made only of `NAME=value` words sets variables (`cock::shell::Variables`, 16 variables, names up to 31 bytes, values up to 63); `set` prints them and `unset name...` removes them. Variables belong to the shell: programs do not receive them (there is no `export`), and there is no `PATH` because programs are only looked for in `/bin`. An expanded line longer than 128 bytes prints `sh: line too long`. `$?` is changed by programs, not by built-ins.
* `cat` writes the bytes as they are, so a binary file prints garbage on the screen.

## Testing
`scripts/qemu_userland_test.sh` types its commands through the QEMU monitor: `echo`, `uname`, `ps`, `ls /`, `cat /etc/mounts.conf`, `ls /ext`, `echo` redirected to `/ext/scratch/sh.txt`, `cat`, `mv`, `cat`, `rm`, a `cat` that fails, `mkdir` and `ls` of a new directory, `rmdir` twice (the second fails), `cd` and `pwd`, four runs of the test program `exitwith` (`test/userland/exitwith`, which prints its arguments and ends with the number it is given: `exitwith 7`, `/bin/exitwith 0`, `./exitwith 5` from `/bin`) and `nosuchprogram`, `echo` and `cat` joined by one and by two pipes, `>`, `>>` and `<` into and out of `/ext/scratch/redir.txt` and `copy.txt` (the script reads them back from the ext2 image), `cat < /nowhere`, `touch`, `wc`, `cp`, `head` and `grep` on two files, `grep` of a text that is absent (status 1), `hexdump`, a pipe of `cat`, `grep` and `wc`, `kill` of a process that does not exist and two malformed `kill` calls, and a line ending in `|`, `sleep 1`, `clear` and a last `echo`. It requires the output of each on the serial log (the file written appears twice, and the failure messages once), the last prompt and output on the screen after `clear`, and the usual `e2fsck` of the ext2 volume at the end.
