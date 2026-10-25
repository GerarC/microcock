# names

The name server. A program asks it for the right to talk to a server; it checks the `ipc=` policy of the manifest and gives the caller an `IPC` capability for the label of the server. It replaces the search of a process by name that the kernel used to offer.

## Where it sits

```text
program --CONNECT (sync call)--> names --SYS_CAP_GRANT--> IPC capability for the server
init    --DEFINE, ALLOW, BIND--> names
```

- It is the second boot module (`names` in `USERLAND_BOOT_ORDER`, between `init` and `ramfs`), so its process id is `NAMES_PID` and every program can call it without looking it up. `init` checks the id and prints `init: no name server` and exits if it differs.
- `init` gives it the label `NAMES_SERVICE`, an `IPC` capability over every label that it may pass on (`SYS_CAP_GRANT` with `GRANT_DELEGATE`) and `CAPABILITY_GRANTED`. The server waits for that message before it serves, then prints `names: ready`. Calls that arrive earlier wait in the kernel queue.
- The C library console client is switched off (`CONSOLE_CLIENT = no`): the console client connects through this server, so the server prints through the kernel.
- It holds no port, interrupt or memory capability, and `holdsGrantAuthority` ignores the `IPC` entry, so it cannot label processes, read boot modules or send `CAPABILITY_GRANTED`.

## Protocol

Header `shared/include/cock/abi/names.hpp`; the page [names](../../documentation/userland/names.md) has the tables. A request is a synchronous call of four words: the operation, the part number, a flag and a label in word 0, 12 bytes of a name in words 1 to 3. A name of up to 60 bytes comes in up to five parts and the server answers `MORE` until the last. A short name is the same as the path `/run/<name>`. `CONNECT` comes from any registered program. `DEFINE`, `ALLOW` and `BIND` are accepted only from `ROOT_PID`; anyone else gets `NOT_ROOT`.

The client side is `cock::user::names::connect` (`userland/lib/include/cock/user/names.hpp`).

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `waitForCapabilities`, then the accept and reply loop |
| `src/grant.cpp`, `include/names/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED` from `init` |
| `src/paths.cpp`, `include/names/paths.hpp` | `normalize`: a short name becomes `/run/<name>` |
| `src/assembly.cpp`, `include/names/assembly.hpp` | `addPart`: puts a name together from its parts, per calling thread (host tested) |
| `src/services.cpp`, `include/names/services.hpp` | The table of services: name, label, running process, allowed labels |
| `src/policy.cpp`, `include/names/policy.hpp` | `mayConnect`: the `ipc=` rule, symmetric |
| `src/grants.cpp`, `include/names/grants.hpp` | The pairs (client, label) already granted, so a second connect gives nothing new |
| `src/connect.cpp`, `include/names/connect.hpp` | `connect`: client, server, policy, grant |
| `src/registration.cpp`, `include/names/registration.hpp` | `registerService` (`ALLOW`, `BIND`, which also records the endpoint) and `defineProgram` (`DEFINE`), root only |
| `src/requests.cpp`, `include/names/requests.hpp` | `serve`: dispatch of one call |
| `src/parked.cpp`, `include/names/parked.hpp` | `park`, `answerParked`: keeps the connect call of a program `init` has not registered yet and answers it on `BIND` |
| `Makefile` | `NAME = names`, `SRCS`, a small `LIBC_SRCS` list, `CONSOLE_CLIENT = no` |

## Build, run, test

```sh
make userland/names
make iso
make run-headless
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Use `HOST_TOOLCHAIN=1` without the cross compiler. The end to end script runs `probe`, which checks a refused connection, the right to call after connecting, a connection repeated forty times, a path, a name of several parts, a connection from a second thread and a refused registration. `test/host/test_names.cpp` builds `services.cpp`, `policy.cpp`, `paths.cpp` and `assembly.cpp` on the host and checks them with the wire format.

## Limits and known gaps

- It is a boot module, so nobody restarts it; if it ends, no program can connect again.
- Only a process that `init` registered can connect; the name server asks the kernel (`SYS_THREAD_PROCESS`) which process a calling thread belongs to.
- Names are at most 60 bytes and eight are put together at a time; 48 programs and 128 granted pairs.
- The policy is the `ipc=` list: no owner or mode per name yet.

## Documentation

- [The name server](../../documentation/userland/names.md)
- [Capabilities](../../documentation/core/capabilities.md)
- [init](../init/README.md)
