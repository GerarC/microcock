# Endpoints and handles

An endpoint is a numbered mailbox in the kernel. A program sends to an endpoint through a **handle** it holds, not to a process id. This is step E3 of the path to Unix style IPC; the pieces described here are the kernel side (E3a). The programs still address each other by process id: they move to handles in E3b, and the process id leaves the interface in E3c.

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

`SYS_EP_CREATE` needs grant authority (the root server) and gives the caller the owner capability, with all three rights. `CapabilitySet::holdsGrantAuthority` ignores `ENDPOINT` entries, as it ignores `IPC` ones, so the name server can hold grantable handles without becoming a root.

## The calls

| Call | Number | Does |
|------|--------|------|
| `SYS_EP_CREATE` | `0x244` | Creates an endpoint and returns its id; writes the handle of the owner capability where `EBX` points |
| `SYS_EP_BIND` | `0x245` | Binds endpoint `EBX` to process `ECX`; the previous owner stops receiving |
| `SYS_EP_SEND` | `0x224` | Like `SYS_IPC_SEND`, with a handle in `EBX`; the kernel writes the badge in `Message::badge` |
| `SYS_EP_CALL` | `0x236` | Like `SYS_IPC_CALL`, with a handle in `EBX`; the server reads the badge as the `peer` of its accept |
| `SYS_CAP_GRANT` | `0x240` | With the type `ENDPOINT`: `EDX` is the endpoint id, `EDI` the flags (`GRANT_DELEGATE`, `GRANT_MINT`), `EBP` the badge |

User wrappers: `ep_create`, `ep_bind`, `ep_send` (`cock/user/api.hpp`), `ep_call` (`cock/user/call.hpp`), and `cap_grant_node` and `cap_send` with their `mint` and `badge` (or `tag`) arguments.

## What a message carries

* A message sent through a handle has `badge` set to the badge of the handle, whatever the sender wrote. A message sent by process id, and the notices the kernel makes (interrupts, exits), have `NO_BADGE` (0).
* A synchronous call through a handle puts the badge in the `peer` register of the server's accept. A call by thread id puts the id of the caller, as before. A server that serves both has to tell them apart by what it expects; once every program uses handles (E3c) the id is gone.
* The kernel still writes `sender_pid`. It stays until E3c.

## Routing

`resolveHandle` (`cock/core/syscall/ipc/endpoint_route.hpp`) is the whole lookup: the number must be a node of an `ENDPOINT` capability **of the caller**, the endpoint must be bound, and the process bound to it must have a live first thread. The message then goes to that thread, the same inbox and the same call queue as a send by process id. No service label is compared: holding the handle is the permission, so a number taken from another process, a revoked handle or a stale one leads nowhere, and costs the caller a `-1`.

The endpoint is not tied to a process for life. `Process::~Process` unbinds the endpoints of the process that ends, and `SYS_EP_BIND` gives the endpoint to its replacement. The handles the clients hold keep working and keep their badges, so a restarted server needs no new grant. What does not survive is what was already queued for the dead process: its inbox and its call queue go with it, and the callers of a queued synchronous call get a failure, as for any dead server. A queue that belongs to the endpoint is not part of this step.

## Checked by

* In-kernel suite `Endpoints` (`test/cock/core/security/test_endpoint.cpp`): the table, who may choose a badge, the bind right that never passes on, the handle lookup, `resolveHandle` for an unbound endpoint, a restarted server and a revoked handle; and `checkServerReadsThePeerTheCallerChose` in the synchronous call suite.
* Host tests (`test/host/test_endpoint_table.cpp`): the table, the handle lookup, the flag queries and the grant authority rule.
* Ring 3 (`epgiver` and `epserver`, see `test/userland/README.md`): the badge reaches the server in a call and in a message, a forged badge is replaced, a process with no handle cannot send or call with any of 256 guessed handles, create an endpoint or bind one, a revoked handle fails, and binding the endpoint to another process moves it.
* The syscall fuzzer has an entry for each call.

## Limits

* The table has 64 endpoints, never freed.
* A process holds at most `CapabilitySet::MAX_CAPABILITIES` (16) capabilities, handles included.
* The queue of an endpoint is the inbox and the call queue of the thread bound to it; what is queued when the owner dies is lost.
* Programs do not use handles yet: `init` does not create endpoints and the name server still returns process ids (E3b).
