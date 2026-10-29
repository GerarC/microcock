# hexdump

Shows the bytes of a file in hexadecimal. A program of `/bin`: the shell runs it with `fork`, `exec` and `wait`; it is not built into `sh`.

## Use

```text
hexdump [path]
echo text | hexdump
```

Sixteen bytes to a row: the offset in eight hexadecimal digits, the bytes, and the same bytes between bars with `.` for those that are not printable. The last line is the total size as an offset, as `hexdump -C` does; an empty input prints nothing. Exit status 0 on success, 1 when the input cannot be opened or read or the output cannot be written, 2 for more than one path (it prints `usage: hexdump [path]`).

## Where it sits

It needs the `vfs` server. Its descriptors and working directory come from the shell through `exec`; it has no capabilities and no manifest line, because it is packed into the initrd under `bin/hexdump`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: calls `run` |
| `src/command.cpp`, `include/hexdump/command.hpp` | `run`: reads the input and feeds the rows |
| `src/row.cpp`, `include/hexdump/row.hpp` | `RowWriter`: collects sixteen bytes, formats the row and writes it |
| `Makefile` | `NAME = hexdump`, the `SRCS` and `LIBC_SRCS` lists and `STREAM_CLIENT = yes` |

## Build, run, test

```sh
make userland/apps/hexdump
bash scripts/qemu_userland_test.sh
```

The userland script dumps a 23 byte file and checks the row, the ASCII part and the final offset.

## Limits and known gaps

- Offsets are 32 bit; no options; identical rows are not squeezed.

## Documentation

- [The shell](../../../documentation/userland/apps/shell.md)
- [Userland programs and the manifest](../../../documentation/userland/system/programs.md)
