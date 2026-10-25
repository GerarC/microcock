# Endpoints and handles

An endpoint is a numbered mailbox in the kernel. A program sends to an endpoint through a **handle** it holds, not to a process id. This is step E3 of the path to Unix style IPC: E3a is the endpoint, the handle and the badge, E3b1 the work of `init` and `names`, E3b2 the reply tokens, and E3c the end of the process id as an address: no message, call or capability message reaches another process by its id, and `sender_pid` is gone from the message. The threads of one process still reach each other by thread id (`SYS_IPC_SEND`, `SYS_IPC_CALL`), because a thread is not a security boundary and has no endpoint.

## Why

A process id names whoever has it now. A handle names a right: holding it is the permission to send, a server that restarts keeps its endpoint, and the kernel stamps on every message who sent it. That is what a file descriptor and a socket give in Unix. The endpoint lives in the kernel because a handle is a capability and only the kernel enforces capabilities; nothing else about it needs the kernel.

## The objects

| Object | What it is | Where |
|--------|------------|-------|
| Endpoint | A number from `FIRST_ENDPOINT` (1) up to `MAX_ENDPOINTS` (64), bound to at most one process | `EndpointTable` in `cock/core/task/endpoint_table.hpp` |
| Handle | An `ENDPOINT` capability: its range is the endpoint id (a range of one). The number a program passes is the **node id** of that capability in the derivation tree | `CapabilityType::ENDPOINT`, `CapabilitySet::endpointHandle` |
| Badge | A 32 bit value stored in the handle and stamped in every message sent through it | `Capability::badge`, `Message::badge`, the `peer` of an accept |

The handle is a capability like the others, so it comes with the derivation tree, revocation (`SYS_CAP_REVOKE` on its node, or on an ancestor), the capability message (`CAPABILITY_TRANSFER`, which gives the receiver a handle) and the cleanup when its holder dies.

## Rights

An `ENDPOINT` capability carries these rights in its flags (`cock/core/security/capability.hpp`):

| Flag | Right | Passes on? |
|------|-------|------------|
| `CAPABILITY_FLAG_GRANT` | Give copies of the handle to others (`SYS_CAP_GRANT` with `GRANT_DELEGATE` to let them give on) | As `GRANT_DELEGATE` |
| `CAPABILITY_FLAG_MINT` | Choose the badge of the copies one grants | As `GRANT_MINT`, and only if the giver holds it |
| `CAPABILITY_FLAG_BIND` | Bind the endpoint to a process (`SYS_EP_BIND`) | Never |

A holder with no mint right can only pass its handle on with the badge it has: a client cannot invent a badge for a third party. A handle with none of these rights is plain: it sends and calls, nothing else.

`SYS_EP_CREATE` needs grant authority (the root server) and gives the caller the owner capability, with all three rights, unless the caller already holds a capability with the bind right over the new id: the first program holds one over every endpoint, so it needs no owner capability per endpoint and its table of 16 entries does not fill. `CapabilitySet::holdsGrantAuthority` ignores `ENDPOINT` entries, as it ignores `IPC` ones, so the name server can hold grantable handles without becoming a root.

## The calls

| Call | Number | Does |
|------|--------|------|
| `SYS_EP_CREATE` | `0x244` | Creates an endpoint and returns its id; writes the handle of the owner capability where `EBX` points |
| `SYS_EP_BIND` | `0x245` | Binds endpoint `EBX` to process `ECX`; the previous owner stops receiving |
| `SYS_EP_SEND` | `0x224` | Like `SYS_IPC_SEND`, with a handle in `EBX`; the kernel writes the badge in `Message::badge`. `EDX` holds the flags: `SEND_EXPECT_REPLY` opens a reply token |
| `SYS_EP_REPLY` | `0x225` | Answers the message that carried the token in `EBX`; the message is at `ECX` |
| `SYS_EP_FORWARD` | `0x226` | Passes the message at `EDX`, with the token in `EBX`, to the endpoint of the handle in `ECX` |
| `SYS_EP_CALL` | `0x236` | Like `SYS_IPC_CALL`, with a handle in `EBX`; the server reads the badge as the `peer` of its accept |
| `SYS_CAP_GRANT` | `0x240` | With the type `ENDPOINT`: `EDX` is the endpoint id, `EDI` the flags (`GRANT_DELEGATE`, `GRANT_MINT`), `EBP` the badge |

User wrappers: `ep_create`, `ep_bind`, `ep_send`, `ep_reply`, `ep_forward` (`cock/user/api.hpp`), `ep_call` (`cock/user/call.hpp`), and `cap_grant_node` and `cap_send` with their `mint` and `badge` (or `tag`) arguments.

## What a message carries

* A message sent through a handle has `badge` set to the badge of the handle, whatever the sender wrote. A message sent between the threads of one process, an answer (`SYS_EP_REPLY`) and the notices the kernel makes (interrupts, exits) have `NO_BADGE` (0). Who sent a message is its badge; there is no `sender_pid` field.
* A synchronous call through a handle puts the badge in the `peer` register of the server's accept. A call between threads of one process puts the id of the caller.
* The notices of the kernel are told apart by their type (`IRQ_NOTIFICATION`, `PROCESS_EXITED`), which no program may send.

## The handle a program is born with

A program must reach the name server before it can reach anything else, and it cannot learn the number of a handle from anyone who is not running yet. `SpawnRights` (the third argument of `SYS_PROC_CREATE`, see [capabilities](capabilities.md)) carries one more entry for this: `initialEndpoint` and `initialFlags`. When `SYS_PROC_START` creates the first thread, the kernel derives an `ENDPOINT` capability for that endpoint from the creator (which needs the grant and mint rights over it), gives it the process id of the new process as its badge, and leaves its number in `EAX` of the first thread. `crt0` stores it and `cock::user::initialHandle()` returns it; `cock::user::names::connect` calls the name server through it. There is no message to wait for and no window in which the program runs without it.

`init` gives every program of the manifest a handle on the endpoint of `names`. A program that was started with none (the boot modules, and the children of a program that has no such handle to give) gets `BAD_REQUEST` from `connect`, and `initialHandle()` returns 0.

## Answering without a process id: reply tokens

A badge says who sent a message but is not an address, so a server cannot answer "to badge 7". The answer to an asynchronous request goes through a **reply token**, the idea of the `rcvid` of QNX and of the send-once right of Mach.

1. The sender asks for it: `SYS_EP_SEND` with the flag `SEND_EXPECT_REPLY`. The kernel opens a token in `ReplyTable` (`cock/core/task/reply_table.hpp`) and writes it in `Message::reply` of the message the receiver gets. Without the flag, and for every message sent between the threads of one process, `reply` is `NO_REPLY` (0); whatever the sender wrote there is replaced.
2. The receiver answers with `SYS_EP_REPLY(token, message)`. The answer is queued for the thread that asked, with `badge` and `reply` cleared, and the token is spent.
3. Or it passes the request on with `SYS_EP_FORWARD(token, handle, message)`: the process bound to the endpoint of the handle receives the message with the same token, and with the badge of the handle, and holds it from then on. It answers the thread that asked directly. `vfs` does not have to wait for `fat` and relay the answer.

The token is a number with a generation: `(generation << 8) | slot`. A token that was spent, dropped or never given does not work, even if its slot was reused. Only the **holder**, the process the message was delivered to, can use it, and only once; the table lives in the kernel, so a token takes no entry of the capability table of the server (which has 16).

What the kernel checks and does:

* `SYS_EP_REPLY` and `SYS_EP_FORWARD` fail for a process that does not hold the token. Reserved message types and `CAPABILITY_TRANSFER` are refused in both.
* If the thread that asked is gone, `SYS_EP_REPLY` closes the token and fails. If its inbox is full it fails and the token stays open, so the server can try again. A forward that cannot be queued leaves the token with the forwarder.
* `Process::~Process` drops every token the process holds or asked for.
* A process can have `MAX_REPLIES_PER_REQUESTER` (16) tokens open as the one that asked, in a table of `MAX_REPLIES` (256), so a server that never answers cannot use up the table. The seventeenth `SEND_EXPECT_REPLY` fails.

## What this does not solve

* **A server cannot tell whether a client is alive.** A badge is a number, and `names` makes it the process id of the client so a server can ask `process_alive`, as `console` does to free the text of a writer that died. That works only while the badge is the process id, and a reused id could be taken for the old client. A notice from the kernel when a client dies is the answer, and it is not made.
* **A client waits forever when the holder dies.** The kernel drops the tokens of a process that ends and tells nobody: the thread that asked gets no answer and no failure. It needs a time limit of its own, or a notice that the kernel does not send yet.
* **A token has no time limit.** A server that keeps a token and never answers holds one of the 16 of the requester until one of the two ends.
* **The answer does not say who gave it.** After a forward the answer comes from the process that replied, not from the server the client sent to, and it carries no badge. The libraries of `fs`, `block` and `rtc` accept a message of the right type, with `badge == NO_BADGE` (so it came through `SYS_EP_REPLY` and not through a handle) and the request id they sent.
* **Shared memory is still shared by process id** (`SYS_SHM_SHARE`), and so are `spawn` and the exit notices. They are not messages, and the badge does not replace them. The right to share is now a handle: `mayShare` (`cock/core/security/ipc_access.hpp`) holds when the two processes are the same, or when one holds a handle on an endpoint the other receives on.
* **A connection to the name server serves a process, not a thread.** The badge of the initial handle is the process id, so two threads of one program that connect at the same moment share the part of a long name that the name server is putting together. A program connects from one thread at a time.

## Routing

`resolveHandle` (`cock/core/syscall/ipc/endpoint_route.hpp`) is the whole lookup: the number must be a node of an `ENDPOINT` capability **of the caller**, the endpoint must be bound, and the process bound to it must have a live first thread. The message then goes to that thread, the same inbox and the same call queue as a send by process id. No service label is compared: holding the handle is the permission, so a number taken from another process, a revoked handle or a stale one leads nowhere, and costs the caller a `-1`.

The endpoint is not tied to a process for life. `Process::~Process` unbinds the endpoints of the process that ends, and `SYS_EP_BIND` gives the endpoint to its replacement. The handles the clients hold keep working and keep their badges, so a restarted server needs no new grant. What does not survive is what was already queued for the dead process: its inbox and its call queue go with it, and the callers of a queued synchronous call get a failure, as for any dead server. A queue that belongs to the endpoint is not part of this step.

## Checked by

* In-kernel suite `Endpoints` (`test/cock/core/security/test_endpoint.cpp`): the table, who may choose a badge, the bind right that never passes on, the handle lookup, `resolveHandle` for an unbound endpoint, a restarted server and a revoked handle; and `checkServerReadsThePeerTheCallerChose` in the synchronous call suite.
* Host tests (`test/host/test_endpoint_table.cpp`): the table, the handle lookup, the flag queries and the grant authority rule.
* Ring 3 (`epgiver` and `epserver`, see `test/userland/README.md`): the badge reaches the server in a call and in a message, a forged badge is replaced, a process with no handle cannot send or call with any of 256 guessed handles, create an endpoint or bind one, a revoked handle fails, and binding the endpoint to another process moves it.
* Host tests (`test/host/test_reply_table.cpp`): open and close, forged and stale tokens, the pass of the holder, the quota, the full table and the tokens dropped when a process ends.
* Ring 3, `epgiver` and `epserver` again: a request with `SEND_EXPECT_REPLY` reaches the server with a token, the server forwards it through a second handle (the forwarded message carries the badge of that handle and the same token), the final receiver answers and the giver gets the answer with the replier as sender; the token is then spent, so a second reply and a forward fail; 1024 token values nobody gave fail; the seventeenth open token of the giver is refused. Mutants that skip the holder check or keep the token after the answer fail the test.
* In-kernel suite `IPC access` (`test/cock/core/security/test_ipc_access.cpp`): `mayShare` through a handle of either side, an unbound endpoint, a third process, revocation cutting both directions, and the initial handle of `SpawnRights` (badge equal to the new process id, refusal without the mint right, for an endpoint the creator does not hold, and for unknown flags).
* Ring 3, `epgiver` again: sending or calling a process of another program by its id fails, and the probe checks the same after `connect`.
* The syscall fuzzer has an entry for each call and, with the `HANDLE` argument kind, uses the handles that the name server gave it.

## Limits

* The table has 256 endpoints, never freed.
* A process holds at most `CapabilitySet::MAX_CAPABILITIES` (16) capabilities, handles included.
* The queue of an endpoint is the inbox and the call queue of the thread bound to it; what is queued when the owner dies is lost.
* The console, the clock, the block driver and the file servers (`ramfs`, `vfs`, FAT32, ext2) answer through reply tokens and key their clients by the badge. `init`, which is not started by anyone, makes a handle for itself on `ramfs` and on `names` with its own process id as badge. `kbd` sends keys and the console draws through handles with no reply token. Nothing is reached by process id across processes any more: the test programs (`phoenix`, `capgiver`, the probe, the fuzzer) use handles too, and `syncbench` and the thread tests of the probe use thread ids inside their own process.
