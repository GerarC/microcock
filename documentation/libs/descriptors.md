# Descriptors in the C library

A program sees input and output as descriptors, small numbers that `read`, `write`, `lseek`, `close`, `dup` and `dup2` take. The table lives in the C library, in the memory of the process: the kernel keeps no table of descriptors, and a server only knows the handles it gave. Does it have to live in the kernel? No: it is bookkeeping of one process over handles that servers already check, so it is a library.

| Part | Where |
|------|-------|
| The table, `dup`, `dup2`, `close`, `lseek`, the dispatch of `read` and `write` | `libc/unistd/descriptor.hpp` and `descriptor.cpp` |
| Console descriptions | `descriptor.cpp`, over `console.cpp` |
| File descriptions | `libc/unistd/file.cpp`, over the `vfs` client |
| Pipe descriptions and `pipe()` | `libc/unistd/pipe.cpp`, over the client of the [pipe server](../userland/pipe.md) |
| Test | `test/userland/cfile` (`cfile: descriptors ok`) and `test/userland/pipeclient` |

## Descriptors and descriptions
A descriptor is an index into a table of 32. Each used entry names an **open file description**, which holds the kind of object (an `Operations` table of `read`, `write`, `seek` and `close`), the handle at the server, the offset, and the access mode. `dup` and `dup2` make a second descriptor name the same description, so the two share the offset and the mode, as POSIX says. A description counts its descriptors and calls its `close` only when the last one goes, so the server's handle is released once.

Descriptors 0, 1 and 2 are born as console descriptions (0 readable, 1 and 2 writable). They are ordinary entries: `close(1)` frees it, and `dup2(fd, 1)` makes everything written to standard output, `printf` included, go to `fd`.

## What a call does
* `read` and `write` check that the descriptor is open for that access (`EBADF`), that the buffer is not null (`EFAULT`) and that the count is not zero for `read`, then call the description.
* `lseek` calls the description; the console refuses it with `ESPIPE`.
* `open` asks `vfs` for a handle and, if the table is full, closes the handle again and fails with `EMFILE`.
* When `vfs` stops answering (`NOT_CONNECTED`), every file description is dropped without a call to the server, because its handles mean nothing any more.
* `pipe` asks the pipe server for a pipe and gives the lowest two free descriptors to a read description and a write description; a pipe description closes its end at the server when the last descriptor goes. When the server stops answering, every pipe description is dropped like the file ones.
* A description of a kind whose client a program did not link is dropped when `exec` hands the table on. A program that may be started with its input or output redirected (every program of `/bin`) sets `STREAM_CLIENT = yes` in its `Makefile`, so `common.mk` links the file, working directory and pipe clients; without them `echo hi > file` would find descriptor 1 closed.
* A program that opts out of the console client (`CONSOLE_CLIENT = no`) has no table: `read` and `write` call `SYS_READ` and `SYS_WRITE` with the number they were given.

## Fork and exec
A `fork` copies the table with the address space, and the servers that keep state for each process (`vfs` for files, `pipe` for pipe ends) are told of the copy and give it twins of what the parent holds, under the same numbers, so every inherited descriptor works in the child. The copy shares nothing with the parent in the library: the offset of a file is a copy, so after the fork the two offsets move apart (an offset shared across processes would have to live at the server). The first file call of the child connects to `vfs` again to use the window of its own session (`file.cpp` notices that its process id changed), and the pipe client needs nothing.

An `exec` keeps the process id, so the servers keep their rows. The program that replaces its image writes its table into the argument block (`abi::exec::ArgsBlock`: every description with its kind, handle, offset and mode, and which descriptors share which), `pm` writes the block in the new image, and the start code of the new program rebuilds the table before the constructors run (`__descriptors_export` and `__descriptors_import` in `descriptor.cpp`; a program that did not link the table ignores the block). A description of a kind the new program did not link is dropped. The new image connects to `vfs` with `CONNECT_KEEP_FILES`, so its files stay open.

## Buffer of standard output
`printf`, `puts` and `putchar` append to a 512 byte buffer (`libc/unistd/stdout.hpp` and `stdout.cpp`, guarded by a `Mutex`) instead of writing each piece to descriptor 1, so a line costs one message to the console server instead of one per character, and a colored line is never split by another process. The buffer is written out with one `write` when it is full and, when descriptor 1 is a console description (`__descriptor_is_terminal`), when the text just added holds a newline: a pipe or a file gets full blocks. It is also written out before a `write` to any descriptor (so standard error stays in order with standard output), before a `read` (so a prompt without a newline shows), before a `fork` and an `exec` (so the child does not print the text again), in `exit_process`, and before a `close` or `dup2` that replaces descriptor 1 (so the text goes where it was printed). `_exit`, a signal and a fault do not write it: what it still holds is lost. A program with `CONSOLE_CLIENT = no` does not link `stdout.cpp` and writes straight to descriptor 1, as before.

## Current directory
The C library keeps the working directory of the process next to the table (`libc/unistd/cwd.cpp`). A process is born in `/`. `chdir(path)` resolves the path against the directory with `cock::path::resolve` (`shared/include/cock/path/resolve.hpp`, host tested: it understands `.` and `..`, collapses repeated slashes, and `..` of `/` is `/`), opens the result at `vfs` and asks `stat` whether it is a directory (`ENOENT`, `ENOTDIR` or `ENAMETOOLONG` otherwise), and only then stores it. `getcwd` copies it out. `open`, `unlink`, `mkdir`, `rmdir`, `rename` and `link` resolve their paths the same way, so the file servers still see absolute paths of at most 127 characters. A `fork` copies the directory with the address space; an `exec` writes it into the argument block (`ArgsBlock::cwd`, empty when none is handed on) next to the descriptors, and the start code of the new program takes it back before the constructors run. Links are not followed while `..` is resolved: that is text work, and the servers resolve links.

## Limits

32 descriptors per process, and as many ends and files as the servers hold (64 pipe ends, 16 open files). A thread of the parent other than the one that forks is not stopped: what it opens or closes during the fork is not covered.
