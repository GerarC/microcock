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
* A program that opts out of the console client (`CONSOLE_CLIENT = no`) has no table: `read` and `write` call `SYS_READ` and `SYS_WRITE` with the number they were given.

## Limits
32 descriptors per process; a `fork` copies the table with the address space, but the handles at the servers belong to the parent until the servers copy them (step E5c of the roadmap).
