# ELF loader in Ring 3

A program started at run time is loaded in Ring 3, not by the kernel. The code that knows the ELF format is split in two parts, and neither of them is in the kernel:

| Part | Where | Job |
|------|-------|-----|
| `cock::elf` (`shared/include/cock/elf/image.hpp`) | Shared header | Reads an untrusted image and lists what has to be loaded. It only reads |
| `cock::user::spawn` and `load_segments` (`userland/lib/include/cock/user/spawn.hpp`) | User library | Builds a process from that list with the `SYS_PROC_*` calls |

The kernel keeps a second ELF loader, for the Multiboot boot modules only (see [ELF loader in the kernel](../core/elf_loader.md)). No program reaches it.

Who uses these two parts: `rs` starts the programs of the manifest with `spawn`, `pm` starts programs when a client asks (with `spawn`) and replaces the image of a process for [exec](../core/process/exec.md) (it parses the image with `elf::parse` and loads it with `load_segments`), and the fuzzer feeds both with damaged images (see [fuzzer](../userland/testing/fuzzer.md)).

## The parser

`elf::parse(data, size, imageLimit, out)` never maps or writes memory, and has no dependencies, so a mistake in it can make a program fail to start but cannot reach the kernel, which checks every range again. The host tests run it on images they build by hand (`test/host/test_elf_image.cpp`).

It accepts a 32 bit executable of this architecture and nothing else:

* **Header:** the magic, the class (`ELF_CLASS`), the byte order, the version, the type (executable at fixed addresses) and the machine (`ELF_MACHINE`, from `cock/abi/executable.hpp`); a program header entry of the right size, at least one, and the whole table inside the image.
* **Every loadable segment with bytes in memory:** the size in the file does not exceed the size in memory, the contents lie inside the image, and the segment starts above the first page and ends at or below `imageLimit`, all without overflow. A segment with no bytes in memory is ignored.
* **The image as a whole:** at most `MAX_SEGMENTS` (8) segments, **no two segments sharing a page**, and an entry point inside one of them.

`imageLimit` is the user space limit minus the user stack, so a segment can never reach the stack that the kernel maps.

The result is an `elf::Image`: the entry point and a list of `elf::Segment` (address, file offset, size in the file, size in memory, writable or not). `firstPage`, `pageCount` and `sharePage` give the pages a segment covers.

## Building the process

`spawn(image, size, name, notify_exit, rights)` calls `parse` first, and a bad image returns `NO_PROCESS` without touching the kernel. Then:

1. `proc_create` makes the empty pending process and records the name, the exit notice flag and the rights (see [spawn](../core/process/spawn.md)).
2. `load_segments` takes each segment in file order: `proc_map` maps zeroed writable pages, `proc_write` copies the bytes of the file (the rest stays zero), and `proc_protect` makes the pages read only when the segment is not writable.
3. `proc_start` checks the entry point and creates the first thread, and the call returns the process id.

If any step fails, `proc_discard` destroys the half built process, so the caller never keeps one. A caller that dies in the middle loses its pending process too, because the kernel ties it to its creator.

## Differences with the kernel loader

| | Ring 3 (`cock::elf`) | Kernel (`core/loader`) |
|---|---|---|
| Classes | ELF32 only | ELF32, and ELF64 where the architecture runs it |
| Two segments in one page | Refused | Accepted: the page stays writable if any segment on it is |
| Segments | At most 8 | No fixed limit |
| Who checks the ranges | The parser, and then the kernel again at each `SYS_PROC_*` call | The loader, once |
| Writes to the new memory | By the kernel on behalf of the caller (`SYS_PROC_WRITE`) | Directly, in the target address space |

Because no page is shared, each page has one segment and one permission, so the user loader seals a read only segment page by page with no case to handle for a page that a writable segment also touches.

## Failure

Nothing the parser reads can crash the loader, and nothing the loader asks can harm the kernel: a broken image ends as `NO_PROCESS` for the caller that spawned it. That is why `rs` and `pm` can load files they do not trust.
