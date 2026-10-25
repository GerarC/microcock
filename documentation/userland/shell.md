# The shell

`sh` (`userland/apps/sh`) is the first program a person talks to. It prints a prompt, reads a line from the [console server](console.md), splits it into words and runs a built-in command. It is an ordinary Ring 3 program: it has no capabilities, it reaches files through the `vfs` client (`cock/user/fs.hpp`) and the screen and keyboard through the C library.

## Starting it
`initrd/manifest.conf` starts `sh` last, after `vfs`, `ext` and `kbd`. At boot the screen shows the version and the prompt:
```
microcock v0.2.0 shell, type help
cock>
```
Type on the keyboard of the QEMU window. Without a disk only `/` exists (the `initrd`, read only); `make disk` and `make run DISK=build/disk.img` attach a disk so `/fat` and `/ext` can be listed and written (see the README).

## Commands
| Command | What it does |
|---------|--------------|
| `help` | Lists the commands |
| `version` | Prints `microcock vMAJOR.MINOR.PATCH` (the numbers come from the `Makefile`) |
| `echo text...` | Prints the words separated by single spaces |
| `cd [path]` | Changes the current directory (`/` without a path). It fails with `sh: cd: path: not found`, `not a directory` or `path too long` and leaves the directory as it was. A built-in, because the directory belongs to the process |
| `pwd` | Prints the current directory |
| `ls [path]` | Lists a directory (the current one by default); a name ending in `/` is a directory; a file prints its own path |
| `cat path` | Prints a file |
| `write path text...` | Creates or replaces a file with the text and a newline |
| `mkdir path`, `rmdir path` | Makes a directory, removes an empty one |
| `rm path` | Deletes a file |
| `mv from to` | Renames or moves; both paths must be on the same volume |
| `ps` | Prints the table of the [process manager](pm.md), one line per process: `PID PPID STAT`. `STAT` is `R` (a thread is ready or running), `S` (every live thread waits) or `X` (its exit notice arrived); `T`, `Z` and `D` are printed once something produces them, see [process states](pm.md#process-states). A process with no parent shows `-`. It reads the rows with the `LIST` operation through a handle on `pm` (the manifest line of `sh` has `ipc=pm`); the shell itself shows `S`, because it waits for the reply of `pm`. No name yet: it comes with the process table item of the roadmap |
| `sleep seconds` | Waits, up to 3600 seconds |
| `clear` | Clears the screen (the `CLEAR` operation of the console protocol) |

The shell has a current directory, like any process (`libc/unistd/cwd.cpp`; it starts in `/`). A path that does not start with `/` is taken from it, and `.` and `..` are understood (`..` of `/` is `/`). Every command resolves its paths with `cock/path/resolve.hpp` (`sh/paths.hpp`) before the file server sees them, so the servers get absolute paths only. A failure prints `sh: command: path: reason`, with the reason taken from the file server status (`not found`, `read only`, `already exists`, `not empty`, `different volumes`, and so on). An unknown command prints `sh: name: command not found`.

## How a line is read
The console server edits the line (echo and Backspace) and delivers it whole, with its newline, when Enter is pressed. `sh` calls `read(0)` in a loop, 12 bytes at a time, until it sees the newline. The call blocks in the kernel until a line is ready (see [console](console.md#what-the-server-does)). A line holds at most 127 characters and 8 words (`cock/shell/words.hpp`, tested on the host in `test/host/test_shell.cpp`).

## Limits
* No `spawn`: the shell cannot run other programs yet. That needs a capability to call `spawn` (stage 9) or a request to `rs`.
* No quoting, pipes, redirection, history, tab completion or Ctrl+C.
* Links are not looked at when `..` is resolved: `cd link/..` goes to the parent of the link's name, not of its target.
* `/` is the `initrd` and is read only, so `write`, `mkdir`, `rm` and `mv` only work under `/fat` and `/ext` and need a disk. A writable `/tmp` in memory is noted in the roadmap (`tmpfs`).
* `cat` writes the bytes as they are, so a binary file prints garbage on the screen.

## Testing
`scripts/qemu_userland_test.sh` types fifteen commands through the QEMU monitor: `echo`, `version`, `ps`, `ls /`, `cat /mounts.conf`, `ls /ext`, `write` to `/ext/scratch/sh.txt`, `cat`, `mv`, `cat`, `rm`, a `cat` that fails, `sleep 1`, `clear` and a last `echo`. It requires the output of each on the serial log (the file written appears twice, and the failure message once), the last prompt and output on the screen after `clear`, and the usual `e2fsck` of the ext2 volume at the end.
