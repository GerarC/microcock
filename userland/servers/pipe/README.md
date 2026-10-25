# pipe

The pipe server. It holds the pipes of all processes: a ring of bytes for each pipe, and the read and write ends each process holds. A reader waits while a pipe is empty, a writer while it is full, and both are answered later with a deferred reply. The design, the protocol and the limits are in [the pipe page](../../../documentation/userland/pipe.md).

## Where it sits

```text
program --pipe(), read, write (libc)--> cock::user::pipe::Client --ep_call--> pipe
```

- No capability: `pipe` touches no port or device. The manifest line is `pipe path=servers/pipe restart=3 notify=fork ipc=console` (it prints through `console`, and `notify=fork` makes `rs` tell it about every copy made by `fork`). A program that uses pipes lists `pipe` in its `ipc=`.
- `rs` starts it through `pm`; it prints `pipe: ready` and serves calls on its endpoint. The caller is identified by the badge of its handle, which is the process id.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the accept loop; replies, retries the waiting callers, sweeps when a wait times out |
| `src/pipes.cpp`, `include/pipe/pipes.hpp` | `Pipe`, `createPipe`, `pushBytes`, `popBytes`, `releaseIfUnused` |
| `src/ends.cpp`, `include/pipe/ends.hpp` | `End`, `findEnd`, `createEnds`, `closeEnd`, `releaseDeadOwners`, `anyEnd` |
| `src/requests.cpp`, `include/pipe/requests.hpp` | `serve` and `attempt`: the four operations |
| `src/waiters.cpp`, `include/pipe/waiters.hpp` | `holdCall`, `retryWaiters`, `sweepDead`: callers held with `SYS_IPC_DEFER` |
| `src/reply.cpp`, `include/pipe/reply.hpp` | `answer`: the header word of a reply |
| `src/inbox.cpp`, `include/pipe/inbox.hpp` | `discardMessages`: drops asynchronous messages |
| `include/pipe/limits.hpp` | Sizes of the tables |
| `Makefile` | `NAME = pipe`, `SRCS` under `src/` and a small `LIBC_SRCS` list |
| `../../lib/include/cock/user/pipe.hpp` | The client |
| `../../../shared/include/cock/abi/pipe.hpp` | The protocol |
| `../../../libc/unistd/pipe.cpp` | `pipe()` |

## Build, run, test

```sh
make userland/servers/pipe
bash scripts/qemu_userland_test.sh
```

Use `HOST_TOOLCHAIN=1` without the cross compiler. The end to end script runs `pipeclient` (`test/userland/pipeclient`), which checks a round trip, the end of the data, `EPIPE`, `dup` of an end, standard output redirected into a pipe, a reader woken by a late write, a writer held by a full pipe and the refusals of the server.

## Limits and known gaps

- 16 pipes of 512 bytes, 64 ends, 16 waiting callers; 12 bytes per call.
- A forked child owns twins of the ends of its parent (`notify=fork`); a thread of the parent other than the one that forked can still change the ends meanwhile.
- Ends of a process that died are closed within about 50 ms, not at once.
