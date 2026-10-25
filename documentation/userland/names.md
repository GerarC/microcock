# The name server

`names` is the program a client asks when it wants to talk to a server. It replaces the search of a process by name that the kernel used to offer (`SYS_PROC_LOOKUP`), and it adds the permission check that the search never had: the right to talk to a server is given when the client connects, and only when the manifest allows it.

It is the first step of the route to Unix style IPC. In Unix the descriptor is the capability: it is obtained by connecting to a stable name, the permission is checked once, and a server that restarts keeps its name. Here the stable name is the program name, the capability is a handle on an endpoint of the kernel (see [endpoints](../core/endpoints.md)), and the name server is the one that joins the two. The later steps (endpoints owned by `rs`, inheritance) keep the same split: the kernel enforces, a Ring 3 server decides.

## Why it lives in Ring 3
The kernel must enforce a capability, but it does not have to know which program is called what or who may talk to whom. That is a table of names and a rule read from the manifest, which is data. A bug in the name server can refuse a connection, or give a client a handle on a program the manifest does not allow; it cannot give anything else, because the server can only pass on `ENDPOINT` capabilities, and it cannot touch ports, interrupts, memory or processes (`holdsGrantAuthority` ignores `ENDPOINT` entries). The kernel keeps one narrow call for `rs`, `SYS_MODULE_PROCESS`, to find the processes it started itself from boot modules.

## How a program connects
1. The program calls `cock::user::names::connect("console")` (`userland/lib/include/cock/user/names.hpp`). The name goes in synchronous calls of four words through the **initial handle** of the program, the handle on the endpoint of the name server that the kernel gave it when it started (`cock::user::initialHandle()`, see [endpoints](../core/endpoints.md#the-handle-a-program-is-born-with)), 12 bytes of the name in each, so a short name is one call and a path such as `/run/console` is one or two. Calls never touch the inbox of the caller, so it is safe in the middle of a server loop.
2. The badge of that handle is the process id of the caller, written by the kernel, so the name server knows who asks without trusting the message. It finds that process among the ones `rs` registered, or among the copies made by `fork` that `rs` told it about (`Op::FORKED`, `names/forked.hpp`): a copy has the policy of the service it copies, so it may connect to what its parent may, and to nothing else. If `rs` has not registered it yet (a program that was just started can run before `rs` reaches the next instruction), the answer is `UNKNOWN_CLIENT` and the client library asks again a few times.
3. It puts the name together, adds `/run/` to a name that does not start with a slash, finds the server by that path and applies the policy: the connection is allowed when the `ipc=` list of the caller names the server, or the `ipc=` list of the server names the caller, **and** the owner and mode of the server let the caller in (`names/policy.hpp`, `permits`). The owner is the program in the `owner=` key of the manifest line of the server, and every other program is "other", as in the permission bits of a Unix file: the owner needs the first character of `mode=` to be `x`, any other program the second. A line with no `mode=` allows both (`xx`), so a manifest without the new keys behaves as before. A copy made by `fork` counts as the program it copies.
4. It gives the caller a **handle** on the endpoint of the server (`rs` makes one per program and binds it to the running process), with `SYS_CAP_GRANT` and the process id of the caller as the badge (names holds the mint right over every endpoint, delegated by `rs`). The handle is kept with the grant, so asking again returns the same one and uses no new slot of the capability table and no new node of the derivation tree, and it keeps working when the server restarts.
5. It answers with the process id of the server, its label and the handle (word 3 of the reply). A client sends and calls through the handle (`ep_call`, `ep_send`); the process id only tells it whether the server is alive.

If the server restarts, its process id changes and its endpoint does not. A client that holds the handle keeps it and needs no new grant; it connects again only to learn the new process id.

## Protocol
`shared/include/cock/abi/names.hpp`. A request is four words: word 0 is the header (the operation in bits 0 to 7, the number of the part in bits 8 to 15, the `LAST` flag in bit 16 and a service label in bits 17 to 31); words 1 to 3 carry 12 bytes of a name or numbers. A name of up to 60 bytes travels in up to five parts, one call each, in order; the name server keeps the bytes of each calling process (eight at a time) and answers `MORE` to every part but the last. The reply has the status in word 0, the process id in word 1 and the label in word 2.

A name that starts with `/` is a path; any other name is short for `/run/<name>`, so `console`, `/run/console` and the manifest entry `console` are the same. Paths of other kinds are stored as they are.

| Operation | Who | Meaning |
|-----------|-----|---------|
| `CONNECT` | Any registered program | Ask for the right to talk to the server with the name in the parts |
| `DEFINE` | `rs` | A program exists: label in the header, name in the parts (empty when it cannot be reached by name) |
| `ALLOW` | `rs` | The program with the label of the header may talk to the label in word 1 (one line of an `ipc=` list) |
| `OWNER` | `rs` | The program with the label of the header is owned by the label in word 1 (`NO_SERVICE` for none) and has the mode in word 2 (`MODE_OWNER` and `MODE_OTHER` bits); `rs` sends it for every entry with `owner=` or `mode=` |
| `BIND` | `rs` | The process id in word 1 now runs the program with the label of the header, and receives on the endpoint in word 2; `rs` sends it after every start, including a restart |

| Status | Meaning |
|--------|---------|
| `OK` | Done |
| `NOT_FOUND` | No program has that name, or it is not running |
| `DENIED` | The policy does not let the caller talk to it: the `ipc=` lists name neither program, or the owner and mode of the server refuse the caller |
| `UNKNOWN_CLIENT` | The caller is not a process `rs` registered. A program that has just started is not refused: the server keeps its call until `rs` registers it (`names/parked.hpp`, up to 8 calls, answered when the `BIND` arrives) and the client library waits at most `CONNECT_WAIT_MS`, so the refusal comes only from a program `rs` never registers |
| `NO_ROOM` | The handle could not be given: the table of the caller or the derivation tree is full |
| `BAD_REQUEST` | Unknown operation, empty or too long name, parts out of order, a label out of range, a duplicate, or a full table |
| `NOT_ROOT` | A registration came from a process other than `rs` |
| `MORE` | A part of a name was kept; send the next |

A program whose manifest name is longer than 60 bytes is registered without a name: `rs` prints `rs: <name> cannot be reached by name` and it can still connect to others. The owner and the mode are per program and only gate `connect`: a handle that was given stays valid, and the kernel compares no owner.

## What `rs` does
1. Finds the boot module `names` (`SYS_MODULE_PROCESS`), gives it the label `NAMES_SERVICE` and an `ENDPOINT` capability over every endpoint with the right to mint (`SYS_CAP_GRANT` with `GRANT_DELEGATE` and `GRANT_MINT`), binds it to its endpoint, makes the handle `rs` calls it through, sends it `CAPABILITY_GRANTED` and registers `ramfs`, which has no manifest entry. The name server has no fixed process id: it is found by its module name and reached by its endpoint, so a restarted name server would receive on the same one.
2. After reading the manifest, sends one `DEFINE` per entry, one `ALLOW` per name of each `ipc=` list and one `OWNER` per entry with `owner=` or a `mode=` other than `xx` (`rs: <program> cannot be owned by <name>` when the owner is no entry).
3. Starts every program with a handle on the endpoint of the name server as its initial handle (`SpawnRights::initialEndpoint`), so the program can connect from its first instruction.
4. Sends a `BIND` after each start, so the name server knows which process runs each program.

The name server is reachable by every program that `rs` started, through its initial handle. `rs` talks to `ramfs` and to the boot modules through the handles it makes for itself.

## Limits
- The name server is a boot module, so the kernel starts it and `rs` does not restart it. If it dies, no program can connect again; the ones already connected keep working.
- A program that `rs` did not start (a child of a program holding `SPAWN`) has no initial handle, so it cannot connect; it gets `BAD_REQUEST` until the process manager gives its children one and registers them.
- Two threads of one program that connect at the same moment share the slot of the name being put together, because the badge is the process; a program connects from one thread at a time.
- A name is at most 60 bytes and the server puts together eight names at a time (a ninth caller waits until a slot of a dead process is free: it gets `BAD_REQUEST`).
- The table holds 48 programs and remembers 128 granted pairs, and drops the pairs of processes that ended when it is full.
- A name server that is slow or blocked blocks every connect, but never a connected exchange.

## Tests
`test/host/test_names.cpp` covers the wire format, the table and the policy. `probe` checks from Ring 3 that a connection the manifest does not allow is refused, that a path and a name of several parts work, that a second thread can connect, that the kernel refuses a call or a message to another process by its id, that connecting gives a handle to call through and the right to share memory, that asking forty times does not run out of slots, and that a registration from a program is refused. The end to end run and both fuzzers (the fuzzers connect through it to find their targets) exercise it in every boot.

See also [names README](../../userland/names/README.md), [rs](../../userland/rs/README.md) and [capabilities](../core/capabilities.md).
