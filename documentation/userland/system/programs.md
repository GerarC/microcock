# Programs

The test initrd is `test/initrd/` (manifest lines, the `/mute` mount and the fixture files `hello.txt` and `docs/note.txt`); only `scripts/qemu_userland_test.sh` builds with it. The programs that exist only to test the system (`probe`, `hello`, `aborter`, `syncbench`, `contruncated`, `fuzz`, `fsclient`, `fatclient`, `extclient`, `blkclient`, `cfile`, `mute`, `muteclient`) live in `test/userland/`, not in `userland/`. Their Makefiles include `../../../userland/common.mk` and the root Makefile builds both folders into `build/userland`. The helper `write_check.hpp` they share is in `test/userland/lib/include/cock/user/`.

| Program | Role |
|---------|------|
| `rs` | Root server. The only process born with authority; it grants `ramfs` the `initrd`, reads `manifest.conf` through `ramfs` and does what each line says (asks `pm` to start the programs, grants capabilities) and then serves the notices of `pm` for ever, starting the servers again when they end; see [pm](../servers/pm.md) |
| `pm` | Process manager. `rs` creates it from the initrd and it creates every other process after it; it is their parent in the kernel and keeps the table; see [pm](../servers/pm.md) |
| `init` | The first process as Linux sees it (id 1), without authority: starts the programs of the manifest that carry `parent=init` (the shell) and starts them again; see [pm](../servers/pm.md) |
| `pmclient` | Test program: lists the table of `pm`, checks the first rows, its own row and that a caller that is not `rs` is refused; the end to end test requires `pmclient: ok` |
| `sigprobe` | Test program: signals through `pm` (`ipc=pm,console`). A forked child catches terminate and exits with its own status, a second child ignores terminate and interrupt and is ended by kill (status 137), a signal to an ended process is `NOT_FOUND`, an unknown signal is `BAD_REQUEST`, a child may not signal its parent (`DENIED`), a stopped child shows `STOPPED` in `LIST`, does not run (its timer expires meanwhile) and finishes after continue, a stopped child can be killed, and a program can neither call `SYS_PROC_SIGNAL` nor send a `SIGNAL` message itself; the end to end test requires `sigprobe: ok` |
| `forkprobe` | Test program: forks through `pm`, changes a global and a stack variable on each side and checks that each keeps its own value, forks again inside the child (which only works through the handle it inherited), connects the child and the grandchild through the name server (`console` allowed, `vfs` refused, as for the parent), and collects both with `wait`; the end to end test requires `forkprobe: ok` |
| `names` | Name server, the second boot module. `rs` describes the system to it; a program asks it, through the handle it was born with, for a handle on a server; see [names](../servers/names.md) |
| `vga_driver` | Display server. Maps the video memory and draws what it receives by IPC |
| `ramfs` | File server for the `initrd`; see [file system](../servers/fs/filesystem.md) (a boot module, so `rs` can read the manifest) |
| `console` | Console driver: owns the serial port `0x3F8+8`; serves `WRITE`, `CLEAR` and `ECHO` as synchronous calls and draws on the display server; no line logic; see [console](../drivers/console.md) |
| `tty` | Terminal server: the typed line, its editing, the finished lines and the readers that wait for them; no capabilities; the echo goes through `console`; see [tty](../servers/tty.md) |
| `fuzz` | Syscall fuzzer, only started with `FUZZ=user` (from the manifest) or `FUZZ=root` (as the module `rs`); see [fuzzer](../testing/fuzzer.md) |
| `contruncated` | Test program: sends a text to `console` without the closing chunk and exits, to check that the server shows what a dead program had sent |
| `kbd` | PS/2 keyboard driver. Binds IRQ 1, reads port `0x60`, decodes the scan codes and sends characters to the `tty` server |
| `sh` | The shell: prompt, a line read from `tty`, and the built-in commands `help`, `cd`, `pwd` and `exit`; every other command is a program of `/bin` that it runs with `fork`, `exec` and `wait`; see [shell](../apps/shell.md). Started last by the system manifest |
| `uname` | Prints `microcock vMAJOR.MINOR.PATCH`. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `true`, `false`, `date` | Exit status 0, exit status 1, and the date and time from the `rtc` server. Programs of `/bin` that the shell runs, with no manifest line and no capabilities (`date` uses the `rtc` right that the shell passes through `exec`); see [shell](../apps/shell.md) |
| `echo` | Prints its arguments. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `ls` | Lists a directory, through the `vfs` client. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `cat` | Prints files. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `mkdir` | Makes directories. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `rmdir` | Removes empty directories. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `rm` | Deletes files. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `mv` | Renames or moves inside one volume. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `ps` | Lists the processes of `pm`. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `touch`, `cp`, `wc`, `head`, `grep`, `hexdump`, `kill` | Create empty files, copy a file, count lines, words and bytes, print the first lines, print the lines with a text, dump bytes in hexadecimal, and signal processes through `pm`. Programs of `/bin` that the shell runs, with no manifest line and no capabilities; the file ones read through `cock/user/stream.hpp`, a header only reading loop in `userland/lib`; see [shell](../apps/shell.md) |
| `sleep` | Waits a number of seconds. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `clear` | Clears the screen through `console`. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](../apps/shell.md) |
| `probe` | Security probe, split by topic in `security.cpp`, `memory.cpp`, `ipc.cpp`, `output.cpp`, `heap.cpp`, `shared_memory.cpp`, `files.cpp` and `threads.cpp`, `thread_library.cpp`, `sync.cpp` and `startup.cpp`, with the shared constants in `probe.hpp`. Checks from Ring 3 that the kernel denies what it must, and prints `probe: 54/54 checks passed` |
| `hello` | Assembly program that writes a line and exits |
| `fsclient` | Reads a file from the `initrd` and exits; `rs` starts it ten times, one after the other, so the server has to release the sessions of the ones that died |
| `aborter` | Calls `abort()` so the end to end test sees its message and the process being reaped |
| `rtc` | Clock server, the only owner of the CMOS ports `0x70+2` (granted by the manifest); answers `RTC_READ` with the date and time read twice until two reads agree, so two servers can never interleave on the index port; started before `fat` and `ext` |
| `pipe` | Pipe server: holds the pipes and the ends of each process, serves `CREATE`, `READ`, `WRITE` and `CLOSE` as synchronous calls and holds a reader or writer with `SYS_IPC_DEFER` while the pipe is empty or full; no capability; see [pipe](../servers/pipe.md) |
| `ata` | Block server for the first IDE disk: PIO reads, MBR and GPT partitions; started by `rs` with the ports `0x1F0+8` and `0x3F6+1`; see [block storage](../drivers/block_storage.md) |
| `fat` | FAT32 server over the block protocol; see [FAT32](../servers/fs/fat32.md) |
| `vfs` | The file server clients talk to: mount table of `/` (`ramfs`) and `/fat` (`fat`); see [file system](../servers/fs/filesystem.md#the-vfs-server) |
| `fatclient` | Reads files, a long name, a nested file, a multi-cluster file and a listing from `/fat`, and checks the refusals; the end to end test requires its output |
| `ext` | ext2 server over the block protocol; the manifest also starts it as `ext4probe` on the ext4 volume to show that it is refused; see [ext2](../servers/fs/ext2.md) |
| `extclient` | Reads files at every level of the block map, a sparse file, listings, symbolic links and the refusals from `/ext`; the end to end test requires its output |
| `cfile` | Uses only the C library (`open`, `read`, `write`, `lseek`, `close`, `dup`, `dup2`, `unlink`) on files of `/ext` and `/fat` (and checks that `dup` shares the offset, that `dup2` replaces a descriptor and that standard output can be redirected to a file): create, append, overwrite after a seek, truncate, delete and the refusals; it checks `errno` for each refusal, makes, renames and removes directories, and that a message sent to itself before a file call is still there afterwards; it leaves `cfile.txt` for the host to check |
| `pipeclient` | Uses only the C library (`pipe`, `read`, `write`, `dup`, `dup2`, `close`) and the pipe client: a round trip, the end of the data, `EPIPE`, an end kept open by `dup`, standard output redirected into a pipe, a reader woken by a late write from another thread, a writer held by a full pipe, and the refusals of the server; prints `pipeclient: ok` |
| `descprobe`, `descchild` | `descprobe` forks and checks that the child uses the descriptors it inherited: a pipe end (the parent closes its own end right after the fork), an open file with the offset of the parent, standard output redirected to a pipe, and, after an `exec` into `descchild`, the same pipe and file in the new image; prints `descprobe: ok` |
| `mute` | Test server: reads one message and exits without answering |
| `muteclient` | Checks that requests to a dead server fail instead of hanging, through `vfs` and directly |
| `blkclient` | Lists the devices of `ata` and reads the first sector of the first partition; the end to end test checks its output |

The processes see each other through the name server: a program calls `cock::user::names::connect("kbd")` and gets the process id of the server and, if the manifest allows it, the right to talk to it. The kernel finds a process by name only for `rs`, and only among the boot modules (`SYS_MODULE_PROCESS`). See [names](../servers/names.md).
