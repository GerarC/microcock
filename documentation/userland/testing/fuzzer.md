# Syscall fuzzer

The fuzzer is a test program that calls the system calls with plausible and hostile arguments and checks that the kernel survives. It lives in `test/userland/fuzz`, like the other test programs, and the normal initrd never starts it.

## What counts as a failure
A user exception only kills the thread that caused it (the kernel prints `Exception{` and `Thread N killed by exception`), so `Exception{` alone is not a failure: a hostile `thread_create` is meant to fault its new thread. The run fails when:

| Exit | Meaning |
|------|---------|
| 1 | `cock panic` on the serial log (a kernel exception, or a state the kernel broke) |
| 2 | The main thread of the fuzzer was killed by an exception |
| 3 | QEMU ended without the final line (a triple fault, because of `-no-reboot`) |
| 4 | The run did not finish in `FUZZ_TIMEOUT` seconds (a hang) |
| 5 | The kernel did not pass the sanity check that the fuzzer runs at the end |
| 6 | The build failed |
| 7 | A structured phase failed or did not end (see "Structured phases") |

A server that died during the run is printed as `fuzz: server NAME died` and is a finding to look at, but it does not fail the run (`ramfs` dies in root mode because the fuzzer, with authority, sends it a forged `CAPABILITY_GRANTED`).

## Modes
| Mode | How it boots | What it reaches |
|------|--------------|-----------------|
| `user` | `rs` starts `fuzz` from the manifest, next to the real servers, with no capabilities | The unprivileged surface: arguments, messages to `vfs`, `fat`, `ext`, `ata`, `console`, `rtc` and `vga_driver`; hostile requests to the file servers and corrupt images for `exec` (see "Structured phases"); privileged calls are negative tests |
| `root` | `fuzz` takes the place of `rs` as the first module, so it gets every capability; only `ramfs` and `vga_driver` run beside it | `cap_grant`, `irq_bind`, `request_resource`, `module_open`, `module_index`, `spawn` with garbage images and with corrupt ELF images (see "Structured phases"), and `MAP_PHYS` |

`make iso TEST_INITRD=1 FUZZ=user|root` selects the mode: for `user` the initrd rule adds the manifest line (with `ipc=` for the servers it attacks, except `kbd`, so refusals are exercised too), for `root` the ISO rule copies `fuzz.elf` as `rs`. Without `FUZZ`, `fuzz` is built and packed but never started.

## Running it
```bash
bash scripts/qemu_fuzz_test.sh HOST_TOOLCHAIN=1
FUZZ=root SEED=7 ROUNDS=4000 bash scripts/qemu_fuzz_test.sh HOST_TOOLCHAIN=1
```
`FUZZ` is `user` by default, `SEED` 1, `ROUNDS` 2000. `FUZZ_TIMEOUT` (seconds, 90 by default) and `SERIAL_FILE` are also read. `FUZZ_DISK=build/disk.img` (made with `make disk`) attaches the test disk, so `ata`, `fat` and `ext` run and the structured phase reaches them; without it they are reported as not reachable. Extra arguments go to `make`; `FUZZ_TRACE=1` makes the program print every call so a failing round can be bisected. The serial log is `build/fuzz_serial.txt`.

## How a call is made
* `specs.hpp` has a table with the number, a weight and the kind of each argument of every call (`ID`, `ADDRESS_IN`, `ADDRESS_OUT`, `LENGTH`, `FLAGS`, `MILLISECONDS`, `SHM`, `NAME_POINTER`, `ENTRY`, and so on). `exit` is not in it, and neither is `SYS_PROC_SIGNAL`: in root mode the fuzzer holds the `SPAWN` right and the ids it picks include the servers, so a kill would end them; its refusals are checked by `sigprobe` and the in-kernel suite instead.
* `main.cpp` builds each argument: about 70% plausible (its own scratch buffer, a live region, a process id and a handle learned from the name server (`names::connect`; the `HANDLE` argument kind picks among those handles, so `SYS_EP_SEND` and `SYS_EP_CALL` reach real servers) or, in root mode, a process id from `SYS_MODULE_PROCESS` (root mode has no handles, so its sends to other programs by id are refused), a small length) and 30% hostile (null, the kernel half, `0xFFFFFFFF`, a pointer plus a length that wraps past 2^32, ids of dead threads, a stale generation). In 4% of the rounds the call number itself is random.
* Calls that can block get a finite time: `ipc_recv` is preceded by a message the fuzzer sends itself, and every time argument is a few milliseconds or a value the kernel refuses (`0x80000000` and above, never `0xFFFFFFFF`).
* Resources that would run out (shared memory objects, mapped regions) are released every 64 rounds. A `MAP_PHYS` mapping is unmapped at once and never touched.
* The generator is an xorshift32 (`prng.hpp`, tested on the host). It has a compile time seed (`FUZZ_SEED`) and no state outside the program, so the same seed repeats the same sequence of random calls. The scenarios below use several threads, so their interleaving is not repeatable.

## Scenarios
Every 100 rounds the fuzzer runs one of three scenarios, because random single calls cannot reach them:
* **Calls among threads.** Three threads `accept`, `reply`, `defer`, `reply_to`, `reply_wait` and `call` each other and hostile ids with short times, while the main thread calls and sends messages to them. Threads exit while serving, deferring or queued, which exercises the cleanup of `SyncIpc`.
* **Futex storm.** The same threads `futex_wait` with several expected values and times, wake, and wait on an unaligned address.
* **Inbox storm.** Every message type from 0 to 39 with random arguments to every server, and a call with random words to each of them (not to `console`, which would print the garbage).

If the threads of a scenario do not finish in three seconds the fuzzer prints `fuzz: scenario stuck` and the sanity check fails.

## Structured phases
Random words rarely get past the first check of a server or of the loader, so after the random calls the fuzzer runs phases that build well formed input with hostile contents. They run once, with the seed of the run, and end with `fuzz: fz1 ok` or `fuzz: fz1 FAILED`; the script exits with 7 unless the first line is there. The helpers are `file_fuzz`, `loader_fuzz`, `elf_mutator` and `structured` in `test/userland/fuzz`.

* **File server protocol** (`user` mode). For each of `vfs`, `ramfs`, `fat` and `ext` the fuzzer opens a real session (`FS_CONNECT` and the window) and sends 400 requests of every `FS_*` type. The window holds a known path, a full page with no terminator, separators only, zeros or random bytes; the lengths are the real one or a hostile one (zero, `MAX_PATH_BYTES` and its neighbors, the window size and past it, `0xFFFFFFFF`); the handles are real, never opened or random; the offsets and counts are hostile; the open flags include undefined bits; `FS_CONNECT` is repeated with random flags. A server passes when every request is answered or the server is alive and then answers a valid request and accepts a new session. A server that died or stopped answering is `LOST` and fails the run. A server the fuzzer cannot connect to is skipped (with no disk `fat` and `ext` are not there), but at least one must be reached.
* **`exec` with corrupt images** (`user` mode). The fuzzer asks `pm` for 24 copies of itself with `fork`. Each copy builds the seed image of `elf_mutator.hpp` (two segments and a program that exits with status 0), corrupts it, and asks `pm` to put it in place of its own image, eight times or until one is accepted. The first copy sends the seed unchanged and must exit with status 0, so a loader that refuses everything fails. A copy that was replaced runs the corrupt code and either faults (the kernel prints `Thread N killed by exception`, which is expected) or ends; the fuzzer kills every copy after 150 ms and collects it with `wait`. The run fails when `pm` does not give back the status of a copy.
* **`spawn` with corrupt images** (`root` mode, which holds the `SPAWN` right). The same mutator feeds `cock::user::spawn` 600 times, after the seed unchanged, which must start. Before each spawn the fuzzer parses the image itself and checks it with `imageIsSafe`: every segment inside the file and below the user stack, no two sharing a page, and the entry point inside a segment. The run fails when an accepted image is not safe or `spawn` starts what the parser refuses. About one image in six passes the parser; the kernel checks every range again in `proc_map`, `proc_write` and `proc_protect`, so a segment that is large but valid is refused there and the half built process is discarded.

The mutator (`elf_mutator.cpp`) flips bits and bytes, writes hostile words into the headers, moves the offset of a segment out of the file, inflates a segment, cuts or extends the image, copies a program header over another and changes the count and the offset of the table. It is tested on the host (`test/host/test_elf_mutator.cpp`): 30000 mutants never pass `elf::parse` while breaking `imageIsSafe`, and a copy of the parser without the check of the file bounds is caught by the same loop, which is the proof that a deliberately broken bounds check does not go unnoticed.

## Output
```
fuzz: seed 1 rounds 2000 mode user
fuzz: main thread 15
fuzz: round 256
fuzz: scenario served 58 replied 16 deferred 18 replied-to 5 answered 56
fuzz: call 17F ok 24 failed 43
fuzz: fz1 files vfs survived requests 400 answered 400 timed-out 0
fuzz: fz1 exec ok children 24 fork-failed 0 lost 0
fuzz: fz1 ok
fuzz: sanity ok
fuzz: done seed 1
```
The `call` lines count how many random calls of each number succeeded and failed; a number whose `ok` stays at zero in a mode where it should succeed means the generator never reaches that path. The sanity check confirms that the identity of the thread did not change, a message to itself arrives, `mmap` and `munmap` work, and no scenario got stuck.

## What came out of it
* `vga_driver` trusted the length of a `OUTPUT_WRITE_STR` message: a program could send a length far above the 20 bytes the message holds and make the display server read past the message. It was found by reading the server while checking what the inbox storm sends it, not by a crash (the runs so far never sent that exact message), and it now clamps the length to the payload.
* In 12 runs of 3000 to 6000 rounds (seeds 1 to 6 in `user` mode and 1 to 3 in `root` mode) the kernel did not panic, hang or fault the main thread.
* The seed is a constant initialized object (`constinit`), so the generator does not depend on a constructor having run: a generator that was never constructed would make every call number 0 and every run pass with no work. The `call` statistics and the scenario counters exist to show that a run is doing work.

## Current limits
* The structured phases found no failure in the runs so far (seeds 1 to 4 in `user` mode with the disk and 1 to 3 in `root` mode). They send requests to one server at a time and never to two at once.
* `exec` is fuzzed through the image in memory that `pm` receives, not through a file, because there is no writable file system for the fuzzer to put a corrupt file in; the reading of the file is the one that `execprobe` and the file phase cover.
* Files are not mutated either: the contents of a real program on the `initrd` or the disk are not corrupted, only the seed image.
* A server killed by a message is reported, not failed.
