# names

The name server. A program asks it for a server; it checks the `ipc=` policy of the manifest and gives the caller a handle on the endpoint of that server. It replaces the search of a process by name that the kernel used to offer.

## Where it sits

```text
program --CONNECT (call through its initial handle)--> names --SYS_CAP_GRANT--> handle on the endpoint of the server
rs      --DEFINE, ALLOW, BIND--> names
```

- It is the second boot module (`names` in `USERLAND_BOOT_ORDER`, between `rs` and `ramfs`). It has no fixed process id: `rs` finds it by its module name, binds it to an endpoint and reaches it through a handle, and every program `rs` starts gets a handle on that endpoint as its initial handle (`cock::user::initialHandle()`), whose badge is the process id of the program. `rs` prints `rs: no name server` and exits if the module is missing.
- `rs` gives it the label `NAMES_SERVICE`, an `ENDPOINT` capability over every endpoint with the right to mint (`SYS_CAP_GRANT` with `GRANT_DELEGATE` and `GRANT_MINT`) and `CAPABILITY_GRANTED`. The server waits for that message (the badge of the message must be `ROOT_PID`) before it serves, then prints `names: ready`. Calls that arrive earlier wait in the kernel queue.
- The C library console client is switched off (`CONSOLE_CLIENT = no`): the console client connects through this server, so the server prints through the kernel.
- It holds no port, interrupt or memory capability, and `holdsGrantAuthority` ignores the `ENDPOINT` entry, so it cannot label processes, read boot modules or send `CAPABILITY_GRANTED`.

## Protocol

Header `shared/include/cock/abi/names.hpp`; the page [names](../../documentation/userland/names.md) has the tables. A request is a synchronous call of four words: the operation, the part number, a flag and a label in word 0, 12 bytes of a name in words 1 to 3. A name of up to 60 bytes comes in up to five parts and the server answers `MORE` until the last. A short name is the same as the path `/run/<name>`. `CONNECT` comes from any registered program. `DEFINE`, `ALLOW` and `BIND` are accepted only from `ROOT_PID`; anyone else gets `NOT_ROOT`.

The client side is `cock::user::names::connect` (`userland/lib/include/cock/user/names.hpp`).

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `waitForCapabilities`, then the accept and reply loop |
| `src/grant.cpp`, `include/names/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED` from `rs` |
| `src/paths.cpp`, `include/names/paths.hpp` | `normalize`: a short name becomes `/run/<name>` |
| `src/assembly.cpp`, `include/names/assembly.hpp` | `addPart`: puts a name together from its parts, per caller (host tested) |
| `src/services.cpp`, `include/names/services.hpp` | The table of services: name, label, running process, allowed labels |
| `src/policy.cpp`, `include/names/policy.hpp` | `mayConnect`: the `ipc=` rule, symmetric, and `permits`: the owner and mode of the server |
| `src/grants.cpp`, `include/names/grants.hpp` | The pairs (client, label) already granted, so a second connect gives nothing new |
| `src/connect.cpp`, `include/names/connect.hpp` | `connect`: client, server, policy, handle |
| `src/registration.cpp`, `include/names/registration.hpp` | `registerService` (`ALLOW`, `BIND`, which also records the endpoint) and `defineProgram` (`DEFINE`), root only |
| `src/requests.cpp`, `include/names/requests.hpp` | `serve`: dispatch of one call |
| `src/forked.cpp`, `include/names/forked.hpp` | `addForked`, `findForked`, `findClient`: the processes that are copies (`fork`) of a program, which get the policy of the service they copy (`Op::FORKED`, root only) |
| `src/parked.cpp`, `include/names/parked.hpp` | `park`, `answerParked`: keeps the connect call of a program `rs` has not registered yet and answers it on `BIND` |
| `Makefile` | `NAME = names`, `SRCS`, a small `LIBC_SRCS` list, `CONSOLE_CLIENT = no` |

## Build, run, test

```sh
make userland/names
make iso
make run-headless
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Use `HOST_TOOLCHAIN=1` without the cross compiler. The end to end script runs `probe`, which checks a refused connection, a call through the handle after connecting (and a refused call by process id), a connection repeated forty times, a path, a name of several parts, a connection from a second thread and a refused registration. `test/host/test_names.cpp` builds `services.cpp`, `policy.cpp`, `paths.cpp` and `assembly.cpp` on the host and checks them with the wire format.

## Limits and known gaps

- It is a boot module, so nobody restarts it; if it ends, no program can connect again.
- Only a process that `rs` registered can connect, and only one that was started with an initial handle. The badge of the handle is the process id, so two threads of one program that connect at the same moment share the slot of the name being put together.
- Names are at most 60 bytes and eight are put together at a time; 56 programs and 128 granted pairs.
- The policy is the `ipc=` list plus one owner and a two bit mode (owner, other) per program: no groups, no read or write bits, and a handle already given stays valid.

## Documentation

- [The name server](../../documentation/userland/names.md)
- [Capabilities](../../documentation/core/capabilities.md)
- [rs](../rs/README.md)
