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

`init` (`userland/init/main.cpp`) looks the drivers up by name and gives each exactly what it needs, then tells it that it may start:

| Process | Granted |
|---------|---------|
| `vga_driver` | `PHYSICAL_MEMORY`: `0xA0000` to `0xBFFFF` |
| `kbd` | `IO_PORTS`: port `0x60`; `IRQ`: line 1 |

## Delegation
`SYS_CAP_GRANT(target, type, first, count)` lets a process pass part of what it holds to another:

* the caller must hold a capability of that type **with `GRANT`** that contains the range,
* the delegated capability never carries `GRANT`, so authority cannot be passed down a chain,
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

## What each check does
* **Ports** (`SYS_REQUEST_RESOURCE`): the capability must cover the range. Then the kernel refuses the ports of the PIC (`0x20`, `0x21`, `0xA0`, `0xA1`) and the timer (`0x40` to `0x43`), which are the kernel's. Only then are the bits turned on in the permission bitmap of the process, which the CPU enforces from there on (see [GDT](../arch/gdt.md)).
* **Interrupts** (`SYS_IRQ_BIND`): the capability must cover the line; the timer line and the cascade line are reserved; a line has one owner. `SYS_IRQ_ACK` only works for the owner.
* **Physical memory** (`SYS_MMAP` with `MAP_PHYS`): the capability must cover the range **and** none of its frames may be RAM. A process can map a device, but never the memory of the kernel or of another process. Device mappings are marked as not owned, so they are never given back to the frame allocator.

A denied call fails with `-1` and is logged at `trace` level with the process id, so a program that keeps asking cannot flood the log.

## Authority in messages
Two message types carry authority, and the kernel enforces it in `SYS_IPC_SEND`:

* `IRQ_NOTIFICATION` can only come from the kernel; a program that sends one is refused.
* `CAPABILITY_GRANTED` can only be sent by a process that holds at least one capability with the grant right (`CapabilitySet::holdsGrantAuthority`). In practice that is the root server. A driver that waits for it knows it came from there and does not need to check the sender.

## Testing it
`test/userland/probe` runs sixteen checks from Ring 3 that must be denied or must work:

* it asks for a port, an interrupt line, a grant, the video memory and a RAM frame it holds no capability for,
* it sends the two reserved message types and is refused, and sends itself an ordinary message that arrives with its own id as sender,
* it passes the kernel a kernel pointer and an unmapped pointer, and writes a buffer larger than a call accepts,
* it checks that anonymous `mmap` returns zeroed memory that can be released only once, that a request over the quota fails, that a released address is handed out again, and that the table of regions is bounded,
* and it tries to unmap a kernel address.

* it creates a shared memory object, sees it zeroed, keeps data across a map, an unmap and a new map, and cannot map it after releasing it,
* it asks for objects of no pages and of too many pages, for an unknown object and for a share with an unknown process, and is refused,
* and it fills the table of shared memory objects and finds it bounded.

`scripts/qemu_userland_test.sh` requires `probe: 26/26 checks passed`.

## Current limits
* The policy is fixed at boot: the kernel trusts the first program module it loads (the root task, `init` in the shipped boot order; the kernel does not read its name), and the root task decides who gets what.
* Only the delegated capabilities can be revoked, not the ones of the first program. `init` does not use `SYS_CAP_REVOKE` yet: a revocation is only reached from a program that holds an ancestor node, and no shipped program does it. It is exercised by the kernel suite `Capability revocation` and by the root fuzzer.
* A boot module that was already opened is not closed by a revocation.
* The table has a fixed size.
* There is no capability for IPC: any process may send a message to any process id.
* A capability over ports that drive DMA (disk controllers, the PCI configuration ports) lets its owner write anywhere in physical memory, since there is no IOMMU. It is as strong as root and should be granted accordingly.
