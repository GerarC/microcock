# The name server

`names` is the program a client asks when it wants to talk to a server. It replaces the search of a process by name that the kernel used to offer (`SYS_PROC_LOOKUP`), and it adds the permission check that the search never had: the right to talk to a server is given when the client connects, and only when the manifest allows it.

It is the first step of the route to Unix style IPC. In Unix the descriptor is the capability: it is obtained by connecting to a stable name, the permission is checked once, and a server that restarts keeps its name. Here the stable name is the program name, the capability is the `IPC` capability of the kernel (see [capabilities](../core/capabilities.md#the-ipc-capability)), and the name server is the one that joins the two. The later steps (capabilities in messages, endpoints and handles, inheritance) keep the same split: the kernel enforces, a Ring 3 server decides.

## Why it lives in Ring 3
The kernel must enforce a capability, but it does not have to know which program is called what or who may talk to whom. That is a table of names and a rule read from the manifest, which is data. A bug in the name server can refuse a connection, or give a client the right to talk to a program the manifest does not allow; it cannot give anything else, because the server can only pass on `IPC` capabilities, and it cannot touch ports, interrupts, memory or processes (`holdsGrantAuthority` ignores `IPC` entries). The kernel keeps one narrow call for `init`, `SYS_MODULE_PROCESS`, to find the processes it started itself from boot modules.

## How a program connects
1. The program calls `cock::user::names::connect("console")` (`userland/lib/include/cock/user/names.hpp`). The name goes in synchronous calls of four words to the process id `NAMES_PID` (the second boot module), 12 bytes of the name in each, so a short name is one call and a path such as `/run/console` is one or two. Calls never touch the inbox of the caller, so it is safe in the middle of a server loop.
2. The name server asks the kernel which process the calling thread belongs to (`SYS_THREAD_PROCESS`), so any thread of a program may connect, and finds that process among the ones `init` registered. If `init` has not registered it yet (a program that was just started can run before `init` reaches the next instruction), the answer is `UNKNOWN_CLIENT` and the client library asks again a few times.
3. It puts the name together, adds `/run/` to a name that does not start with a slash, finds the server by that path and applies the policy: the connection is allowed when the `ipc=` list of the caller names the server, or the `ipc=` list of the server names the caller. This is the same symmetric rule the kernel applies to a message.
4. It gives the caller an `IPC` capability for the label of the server with `SYS_CAP_GRANT`, once: the pairs already granted are kept, so asking again uses no new slot of the capability table and no new node of the derivation tree.
5. When the server has an endpoint (`init` makes one per program and binds it to the running process), it also gives the caller a **handle** on it, with `SYS_CAP_GRANT` and the process id of the caller as the badge (names holds the mint right over every endpoint, delegated by `init`). The handle is kept with the grant, so asking again returns the same one, and it keeps working when the server restarts. See [endpoints](../core/endpoints.md).
6. It answers with the process id of the server, its label and the handle (word 3 of the reply). A client that uses the handle (`ep_call`, `ep_send`) never needs the process id to send; `console` is the first service whose client does so.

If the server restarts, its process id changes and its label does not. A client that holds the capability keeps it; it only needs to connect again to learn the new process id, and the name server gives no new capability.

## Protocol
`shared/include/cock/abi/names.hpp`. A request is four words: word 0 is the header (the operation in bits 0 to 7, the number of the part in bits 8 to 15, the `LAST` flag in bit 16 and a service label in bits 17 to 31); words 1 to 3 carry 12 bytes of a name or numbers. A name of up to 60 bytes travels in up to five parts, one call each, in order; the name server keeps the bytes of each calling thread (eight at a time) and answers `MORE` to every part but the last. The reply has the status in word 0, the process id in word 1 and the label in word 2.

A name that starts with `/` is a path; any other name is short for `/run/<name>`, so `console`, `/run/console` and the manifest entry `console` are the same. Paths of other kinds are stored as they are.

| Operation | Who | Meaning |
|-----------|-----|---------|
| `CONNECT` | Any registered program | Ask for the right to talk to the server with the name in the parts |
| `DEFINE` | `init` | A program exists: label in the header, name in the parts (empty when it cannot be reached by name) |
| `ALLOW` | `init` | The program with the label of the header may talk to the label in word 1 (one line of an `ipc=` list) |
| `BIND` | `init` | The process id in word 1 now runs the program with the label of the header, and receives on the endpoint in word 2; `init` sends it after every start, including a restart |

| Status | Meaning |
|--------|---------|
| `OK` | Done |
| `NOT_FOUND` | No program has that name, or it is not running |
| `DENIED` | The policy does not let the caller talk to it |
| `UNKNOWN_CLIENT` | The caller is not a process `init` registered. A program that has just started is not refused: the server keeps its call until `init` registers it (`names/parked.hpp`, up to 8 calls, answered when the `BIND` arrives) and the client library waits at most `CONNECT_WAIT_MS`, so the refusal comes only from a program `init` never registers |
| `NO_ROOM` | The capability could not be given: the table of the caller or the derivation tree is full |
| `BAD_REQUEST` | Unknown operation, empty or too long name, parts out of order, a label out of range, a duplicate, or a full table |
| `NOT_ROOT` | A registration came from a process other than `init` |
| `MORE` | A part of a name was kept; send the next |

A program whose manifest name is longer than 60 bytes is registered without a name: `init` prints `init: <name> cannot be reached by name` and it can still connect to others. An owner and a mode per name come with the descriptor layer of the C library.

## What `init` does
1. Finds the boot module `names` (`SYS_MODULE_PROCESS`), checks that its process id is `NAMES_PID`, gives it the label `NAMES_SERVICE` and an `IPC` capability over every label that it may pass on (`SYS_CAP_GRANT` with `GRANT_DELEGATE`), sends it `CAPABILITY_GRANTED` and registers `ramfs`, which has no manifest entry.
2. After reading the manifest, sends one `DEFINE` per entry and one `ALLOW` per name of each `ipc=` list.
3. Sends a `BIND` after each start, so the name server knows which process runs each program.

The root server and the name server are reachable by every process, because their `IPC` capabilities cover every label. `init` talks to `ramfs` and to the boot modules through the process ids it found itself.

## Limits
- The name server is a boot module, so the kernel starts it and `init` does not restart it. If it dies, no program can connect again; the ones already connected keep working.
- A program that `init` did not start (a child of a program holding `SPAWN`) is unknown to the name server and cannot connect until the process manager registers it.
- A name is at most 60 bytes and the server puts together eight names at a time (a ninth caller waits until a slot of a dead thread is free: it gets `BAD_REQUEST`).
- The table holds 48 programs and remembers 128 granted pairs, and drops the pairs of processes that ended when it is full.
- A name server that is slow or blocked blocks every connect, but never a connected exchange.

## Tests
`test/host/test_names.cpp` covers the wire format, the table and the policy. `probe` checks from Ring 3 that a connection the manifest does not allow is refused, that a path and a name of several parts work, that a second thread can connect, that the kernel refuses a message to a server the probe never connected to, that connecting gives the right to call and to share memory, that asking forty times does not run out of slots, and that a registration from a program is refused. The end to end run and both fuzzers (the fuzzer also calls the name server with random words) exercise it in every boot.

See also [names README](../../userland/names/README.md), [init](../../userland/init/README.md) and [capabilities](../core/capabilities.md).
