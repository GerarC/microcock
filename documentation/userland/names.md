# The name server

`names` is the program a client asks when it wants to talk to a server. It replaces the search of a process by name that the kernel used to offer (`SYS_PROC_LOOKUP`), and it adds the permission check that the search never had: the right to talk to a server is given when the client connects, and only when the manifest allows it.

It is the first step of the route to Unix style IPC. In Unix the descriptor is the capability: it is obtained by connecting to a stable name, the permission is checked once, and a server that restarts keeps its name. Here the stable name is the program name, the capability is the `IPC` capability of the kernel (see [capabilities](../core/capabilities.md#the-ipc-capability)), and the name server is the one that joins the two. The later steps (capabilities in messages, endpoints and handles, inheritance) keep the same split: the kernel enforces, a Ring 3 server decides.

## Why it lives in Ring 3
The kernel must enforce a capability, but it does not have to know which program is called what or who may talk to whom. That is a table of names and a rule read from the manifest, which is data. A bug in the name server can refuse a connection, or give a client the right to talk to a program the manifest does not allow; it cannot give anything else, because the server can only pass on `IPC` capabilities, and it cannot touch ports, interrupts, memory or processes (`holdsGrantAuthority` ignores `IPC` entries). The kernel keeps one narrow call for `init`, `SYS_MODULE_PROCESS`, to find the processes it started itself from boot modules.

## How a program connects
1. The program calls `cock::user::names::connect("console")` (`userland/lib/include/cock/user/names.hpp`). It is one synchronous call of four words to the process id `NAMES_PID` (the second boot module), so it never touches the inbox of the caller and is safe in the middle of a server loop.
2. The name server finds the caller by the thread id the kernel stamps on the call. If `init` has not registered it yet (a program that was just started can run before `init` reaches the next instruction), the answer is `UNKNOWN_CLIENT` and the client library asks again a few times.
3. It finds the server by name and applies the policy: the connection is allowed when the `ipc=` list of the caller names the server, or the `ipc=` list of the server names the caller. This is the same symmetric rule the kernel applies to a message.
4. It gives the caller an `IPC` capability for the label of the server with `SYS_CAP_GRANT`, once: the pairs already granted are kept, so asking again uses no new slot of the capability table and no new node of the derivation tree.
5. It answers with the process id of the server. The client sends to that process id as before.

If the server restarts, its process id changes and its label does not. A client that holds the capability keeps it; it only needs to connect again to learn the new process id, and the name server gives no new capability.

## Protocol
`shared/include/cock/abi/names.hpp`. A request is four words: word 0 has the operation in bits 0 to 7 and a service label in bits 8 to 31; words 1 to 3 carry a name of up to 12 bytes or numbers. The reply has the status in word 0, the process id in word 1 and the label in word 2.

| Operation | Who | Meaning |
|-----------|-----|---------|
| `CONNECT` | Any registered program | Ask for the right to talk to the server with the name in words 1 to 3 |
| `DEFINE` | `init` | A program exists: label in the header, name in words 1 to 3 (empty when it cannot be reached by name) |
| `ALLOW` | `init` | The program with the label of the header may talk to the label in word 1 (one line of an `ipc=` list) |
| `BIND` | `init` | The process id in word 1 now runs the program with the label of the header; `init` sends it after every start, including a restart |

| Status | Meaning |
|--------|---------|
| `OK` | Done |
| `NOT_FOUND` | No program has that name, or it is not running |
| `DENIED` | The policy does not let the caller talk to it |
| `UNKNOWN_CLIENT` | The caller is not a process `init` registered, or not the first thread of one |
| `NO_ROOM` | The capability could not be given: the table of the caller or the derivation tree is full |
| `BAD_REQUEST` | Unknown operation, empty or too long name, a label out of range, a duplicate, or a full table |
| `NOT_ROOT` | A registration came from a process other than `init` |

A name is at most 12 bytes because a synchronous call carries 16. A program whose manifest name is longer is registered without a name: `init` prints `init: <name> cannot be reached by name` and it can still connect to others. Path like names (`/run/vfs`) and an owner and mode per name come with the descriptor layer of the C library.

## What `init` does
1. Finds the boot module `names` (`SYS_MODULE_PROCESS`), checks that its process id is `NAMES_PID`, gives it the label `NAMES_SERVICE` and an `IPC` capability over every label that it may pass on (`SYS_CAP_GRANT` with `GRANT_DELEGATE`), sends it `CAPABILITY_GRANTED` and registers `ramfs`, which has no manifest entry.
2. After reading the manifest, sends one `DEFINE` per entry and one `ALLOW` per name of each `ipc=` list.
3. Sends a `BIND` after each start, so the name server knows which process runs each program.

The root server and the name server are reachable by every process, because their `IPC` capabilities cover every label. `init` talks to `ramfs` and to the boot modules through the process ids it found itself.

## Limits
- The name server is a boot module, so the kernel starts it and `init` does not restart it. If it dies, no program can connect again; the ones already connected keep working.
- Only the first thread of a process can connect: a call carries the thread id, and the table is of processes.
- A program that `init` did not start (a child of a program holding `SPAWN`) is unknown to the name server and cannot connect until the process manager registers it.
- The table holds 48 programs and remembers 128 granted pairs, and drops the pairs of processes that ended when it is full.
- A name server that is slow or blocked blocks every connect, but never a connected exchange.

## Tests
`test/host/test_names.cpp` covers the wire format, the table and the policy. `probe` checks from Ring 3 that a connection the manifest does not allow is refused, that the kernel refuses a message to a server the probe never connected to, that connecting gives the right to call and to share memory, that asking forty times does not run out of slots, and that a registration from a program is refused. The end to end run and both fuzzers (the fuzzer also calls the name server with random words) exercise it in every boot.

See also [names README](../../userland/names/README.md), [init](../../userland/init/README.md) and [capabilities](../core/capabilities.md).
