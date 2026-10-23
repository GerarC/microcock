# Capabilities

In MicroCOCK a process starts with no authority over the hardware. Reaching an I/O port, an interrupt line or a piece of device memory requires a **capability**: an entry in the table of the process that says what it may use. The kernel checks the table at every system call that touches the hardware.

## What a capability is

```cpp
struct Capability {
	CapabilityType type;
	uint32_t flags;
	uint64_t first;
	uint64_t count;
};
```

| Type | `first` and `count` | Needed by |
|------|---------------------|-----------|
| `IO_PORTS` | First port and number of ports | `SYS_REQUEST_RESOURCE` |
| `IRQ` | First line and number of lines | `SYS_IRQ_BIND` |
| `PHYSICAL_MEMORY` | First physical address and length in bytes | `SYS_MMAP` with `MAP_PHYS` |
| `BOOT_MODULE` | First module index and number of modules | `SYS_MODULE_OPEN` |
| `IPC` | First service label and number of labels | `SYS_IPC_SEND`, `SYS_IPC_CALL`, `SYS_SHM_SHARE` (see below) |
| `SPAWN` | `SPAWN_FIRST` (0) and `SPAWN_COUNT` (1): the right has no range | `SYS_PROC_CREATE` |

Except for the `initrd` grant to `ramfs`, which `init` makes itself, the grants of the boot are data: they come from `manifest.conf` (see [userland](../userland/userland.md#the-manifest)).

The type numbers are part of the ABI and live in `shared/include/cock/abi/capability.hpp`, so the kernel and userland agree on them.

Every capability describes a **range**. `covers(type, first, count)` is true when one entry of the table contains the whole requested range; a request that only partly fits is denied, and so is one whose end wraps around. Each process holds up to 16 capabilities.

The flag `CAPABILITY_FLAG_GRANT` marks a capability that its owner may pass on.

## Who holds what
At boot, **only the first program module** (`init` in the shipped boot order; the kernel does not read its name) receives capabilities: all the I/O ports, all the interrupt lines, all the physical memory and every boot module, each with the right to grant. Every other process starts with an empty table. Each of these is also recorded as a root node of the derivation tree (see below).

```cpp
static void grant_root_capabilities(Process &process, size_t module_count) {
	grant_root_capability(process, CapabilityType::IO_PORTS, core::hal::IO_PORT_COUNT);
	grant_root_capability(process, CapabilityType::IRQ, core::hal::IRQ_LINE_COUNT);
	grant_root_capability(process, CapabilityType::PHYSICAL_MEMORY,
						  core::hal::PHYSICAL_ADDRESS_SPACE_SIZE);
	grant_root_capability(process, CapabilityType::BOOT_MODULE, module_count);
}
```

`init` (`userland/init/main.cpp`) finds the boot modules with `SYS_MODULE_PROCESS` and gives each exactly what it needs, then tells it that it may start:

| Process | Granted |
|---------|---------|
| `vga_driver` | `PHYSICAL_MEMORY`: `0xA0000` to `0xBFFFF` |
| `kbd` | `IO_PORTS`: port `0x60`; `IRQ`: line 1 |

## Delegation
`SYS_CAP_GRANT(target, type, first, count, flags)` lets a process pass part of what it holds to another:

* the caller must hold a capability of that type **with `GRANT`** that contains the range,
* the delegated capability carries `GRANT` only when `flags` is `GRANT_DELEGATE` (`cock/abi/capability.hpp`); with `flags` zero it never does, so authority is not passed down a chain by accident. Any other value of `flags` is refused. `init` uses the flag once, to let the name server pass on `IPC` capabilities.
* the target must be a user process, and the id must be the id of a process, not of any thread,
* the call returns the id of the node of the derivation tree that records the grant (greater than 0, below `0x7FFFFFFF`), and `-1` on failure. The user wrapper `cap_grant_node` returns the id (`abi::NO_CAPABILITY_NODE` on failure); `cap_grant` returns only success or failure.

## The derivation tree
`CapabilityTree` (`cock/core/security/capability_tree.hpp`) is a table of `MAX_NODES` (256) nodes, with no allocation. Each node records `{id, parent, holder, type, first, count}`: which capability was given out of which, and which process holds it. Every entry of a process table carries the id of its node (`Capability::node`).

* A capability the kernel gives to the first program is a root node (parent none).
* A grant adds a node whose range lies inside the range of its parent, of the same type. The parent is the entry of the caller that carries `GRANT` and contains the range. If the tree is full, the grant fails and nothing is left behind.
* Ids count up and are never reused while the node exists.
* When a process ends, its nodes are removed and their children are attached to the grandparent, so a delegation chain stays connected to a root. Revocation reads the tree (next section).
* The table is touched with interrupts off (system calls) or under an interrupt guard (process destruction).

## Revocation
`SYS_CAP_REVOKE(node)` takes a delegated capability away, with everything derived from it (`revoke_capability` in `cock/core/security/revocation.cpp`).

* **Who may ask.** The caller must hold an ancestor of the node: the process that granted it, a process above that one, or the root task, which holds the root of every chain. A root node (what the kernel gave to the first program) has no ancestor, so nobody can revoke it, and a process cannot revoke a node it holds itself. A refusal returns `-1` and changes nothing.
* **What happens.** First the entry of every node of the subtree leaves the table of its holder and the node leaves the tree. Then, for every value the holder no longer covers through another capability, the effect is undone:

| Capability | Undone |
|------------|--------|
| `IO_PORTS` | The ports are closed in the permission map of the holder (`hal::revoke_hardware_resource`); when the holder is the running process the map is applied at once |
| `IRQ` | A line that a thread of the holder owns is unbound and masked |
| `PHYSICAL_MEMORY` | A region the holder made with `MAP_PHYS` that touches the revoked range is unmapped whole, even if only part of it lost its capability, unless another capability still covers all of it. `Process` keeps the physical base of each such region (`MemoryRegion::device_base`) |
| `BOOT_MODULE` | Nothing is unmapped: a module already opened stays mapped until the holder unmaps it; only opening it again is refused |

* **Cost.** One check per port or line revoked, with interrupts off: revoking a whole port range is about 65536 checks of a 16 entry table. It runs on a rare path and nothing else pays for it.
* **A holder that exited** has no node left and nothing to undo.

The tree is a plain data structure and is tested on the host (`test/host/test_capability_tree.cpp`); the death of a process is tested in the kernel suite (`test_process.cpp`).

## The IPC capability
It decides who may exchange messages, make synchronous calls and share memory with whom. It is the first step of the route to Unix style IPC (see the roadmap): the capability plays the part of the file descriptor, and the stable name is a **service label**.

* **Labels.** A label (`cock/abi/service.hpp`) belongs to a program, not to a running process: `init` gives the label `PROGRAM_SERVICE_BASE` plus the position of the entry in the manifest to every process started from that entry, so a server that restarts comes back with the same label. `Process::serviceId` holds it, and it can be set once. The kernel labels only the first program (`ROOT_SERVICE`); `ramfs` has `RAMFS_SERVICE`, set by `init`. `SYS_SERVICE_SET(pid, label)` (grant authority only) labels a running process, and is used for the boot modules.
* **The capability.** `CapabilityType::IPC` covers a range of labels. The root capability covers all of them; `init` derives single label capabilities from it, so they appear in the derivation tree and can be revoked.
* **The rule** (`mayTalk` in `cock/core/security/ipc_access.hpp`): a process may send to, call or share memory with another when they are the same process, or when it holds an `IPC` capability that covers the label of the other, **or the other holds one that covers its label**. The right is symmetric, so a client given the right to talk to a server also receives the server's asynchronous answers with no extra grant, and revoking the capability cuts both directions. Replies to a synchronous call are not checked.
* **Where it is enforced:** `SYS_IPC_SEND`, `SYS_IPC_CALL` and `SYS_SHM_SHARE`. A refusal returns `-1` and logs at `trace` only. The kernel's own sends (the forwarding of `SYS_WRITE` to the display server, interrupt notices and exit notices) are not user sends and are not checked.
* **Given at spawn: the label.** `SYS_PROC_CREATE` takes an optional `SpawnRights` (the label, and a list of labels the program may talk to, which `init` leaves empty). The kernel sets them before the first thread of the new process can run. Giving a label afterwards leaves a window in which the program already runs and is refused by its own servers; this was seen: `vfs` failed to read `mounts.conf` before `init` had finished.
* **Given at connect: the right to talk.** A program has no `IPC` capability when it starts. It gets one for each server when it asks the name server to connect it (`documentation/userland/names.md`): the name server checks the `ipc=` policy of the manifest and passes on an `IPC` capability for the label of the server with `SYS_CAP_GRANT`. The name server holds one `IPC` capability over every label, delegated by `init` with `GRANT_DELEGATE`; that gives it no other power, because `holdsGrantAuthority` does not count `IPC` entries. Since the right is symmetric, a server that was never asked to connect still answers the clients it has.
* **Policy as data.** The manifest key `ipc=<name>,<name>` lists the programs one may connect to (`userland/init/README.md`). `init` resolves each name to a label and sends the policy to the name server, so the order in which programs start does not matter and a restarted server needs no new grant for its clients: they connect again and find its new process id.
* **Cost.** Two scans of the 16 entry table at most per send, call or share. `syncbench`, which calls between threads of one process, went from about 8400 to 9300 cycles per round trip (three runs) to about 9400 to 14600, with large noise from the emulator.

## The spawn capability
`SYS_PROC_CREATE` needs a `SPAWN` capability, not grant authority; the other `SYS_PROC_*` calls only accept the handle of a process the caller created. The first program receives it with the right to grant, like the other root capabilities, and `init` gives it to a program that lists `caps=spawn` in the manifest. The delegated capability never carries `GRANT`, so a program that holds it can start processes but cannot pass the right on, cannot send `CAPABILITY_GRANTED` messages and cannot label processes freely.

* **Labels.** A spawner may give the new process a label, and derive `IPC` capabilities for it, only for labels it holds a grantable `IPC` capability for (`spawn_rights_valid` in `spawn_rights.cpp`, checked again at `SYS_PROC_START`). A holder of `SPAWN` alone can start a process but only with no label and no rights: otherwise it could start a process under the label of `ata` or `vfs` and receive what their clients send.
* **What it is not.** The child starts with an empty capability table, as before. A holder of `SPAWN` has no hardware capability to give, so the child can do nothing the spawner could not.
* **Checked by** the `spawner` test program (manifest `caps=spawn`), which starts a copy of `hello` and is refused when it asks for a label, and by `probe`, which holds no `SPAWN` and is refused.
* **Not changed.** `SYS_MODULE_INDEX`, `SYS_SERVICE_SET` and the `CAPABILITY_GRANTED` message still require grant authority: they are the root server's job, not a program's.

## What each check does
* **Ports** (`SYS_REQUEST_RESOURCE`): the capability must cover the range. Then the kernel refuses the ports of the PIC (`0x20`, `0x21`, `0xA0`, `0xA1`) and the timer (`0x40` to `0x43`), which are the kernel's. Only then are the bits turned on in the permission bitmap of the process, which the CPU enforces from there on (see [GDT](../arch/gdt.md)).
* **Interrupts** (`SYS_IRQ_BIND`): the capability must cover the line; the timer line and the cascade line are reserved; a line has one owner. `SYS_IRQ_ACK` only works for the owner.
* **Physical memory** (`SYS_MMAP` with `MAP_PHYS`): the capability must cover the range **and** none of its frames may be RAM. A process can map a device, but never the memory of the kernel or of another process. Device mappings are marked as not owned, so they are never given back to the frame allocator.

A denied call fails with `-1` and is logged at `trace` level with the process id, so a program that keeps asking cannot flood the log.

## Authority in messages
Two message types carry authority, and the kernel enforces it in `SYS_IPC_SEND`:

* `IRQ_NOTIFICATION` can only come from the kernel; a program that sends one is refused.
* `CAPABILITY_GRANTED` can only be sent by a process that holds at least one capability with the grant right over hardware or boot resources (`CapabilitySet::holdsGrantAuthority`, which ignores `IPC` entries, so the name server cannot send it). In practice that is the root server. A driver that waits for it knows it came from there and does not need to check the sender.

## Testing it
`test/userland/probe` runs sixteen checks from Ring 3 that must be denied or must work:

* it asks for a port, an interrupt line, a grant, the video memory and a RAM frame it holds no capability for,
* it sends the two reserved message types and is refused, and sends itself an ordinary message that arrives with its own id as sender,
* it passes the kernel a kernel pointer and an unmapped pointer, and writes a buffer larger than a call accepts,
* it checks that anonymous `mmap` returns zeroed memory that can be released only once, that a request over the quota fails, that a released address is handed out again, and that the table of regions is bounded,
* and it tries to unmap a kernel address.

* it creates a shared memory object, sees it zeroed, keeps data across a map, an unmap and a new map, and cannot map it after releasing it,
* it asks for objects of no pages and of too many pages, for an unknown object and for a share with an unknown process, and is refused,
* and it fills the table of shared memory objects and finds it bounded,
* it asks the name server to connect it to the block server and the clock server, which its `ipc=` list does not name, and is refused (`DENIED`); it sees that the kernel refuses its messages to the servers it never connected to; it connects to the console, for which its list has a name, calls it and is answered; it connects forty times, more than its capability table holds, and still succeeds because the capability is given once; it is refused when it tries to register a service, since only `init` may; and it shares memory with the console after connecting.

The in-kernel suites `Capability revocation` and `IPC access` cover revocation and the rule of `mayTalk` with real processes (same process, no capability, the capability of either side, a range that does not cover, the root, revocation cutting both directions, and a label that can be set once).

`scripts/qemu_userland_test.sh` requires `probe: 60/60 checks passed`.

## Current limits
* The policy is fixed at boot: the kernel trusts the first program module it loads (the root task, `init` in the shipped boot order; the kernel does not read its name), and the root task decides who gets what.
* Only the delegated capabilities can be revoked, not the ones of the first program. `init` does not use `SYS_CAP_REVOKE` yet: a revocation is only reached from a program that holds an ancestor node, and no shipped program does it. It is exercised by the kernel suite `Capability revocation` and by the root fuzzer.
* A boot module that was already opened is not closed by a revocation.
* The table has a fixed size.
* The IPC capability is only as precise as the labels `init` hands out; the root task and the name server can be reached by every process, because their capabilities cover every label.
* A program that `init` did not start (a child of a spawner) has no entry in the name server, so it cannot connect to anything until the process manager registers it.
* Only the first thread of a process can connect through the name server: a call carries the id of the calling thread, and the name server knows processes.
* A boot module (`names`, `ramfs`, `vga_driver`) is labeled by `init` after it starts, so it cannot be reached by the programs that wait for no message until `init` has given the label; both only wait for messages.
* A capability over ports that drive DMA (disk controllers, the PCI configuration ports) lets its owner write anywhere in physical memory, since there is no IOMMU. It is as strong as root and should be granted accordingly.
