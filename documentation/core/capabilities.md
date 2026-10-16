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
At boot, **only the module called `init`** receives capabilities: all the I/O ports, all the interrupt lines and all the physical memory, each with the right to grant. Every other process starts with an empty table.

```cpp
static void grant_root_capabilities(Process &process) {
	Capability ports;
	ports.type = CapabilityType::IO_PORTS;
	ports.flags = CAPABILITY_FLAG_GRANT;
	ports.count = core::hal::IO_PORT_COUNT;
	...
	process.getCapabilities().add(ports);
	process.getCapabilities().add(irqs);
	process.getCapabilities().add(memory);
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
* the target must be a user process, and the id must be the id of a process, not of any thread.

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
* The policy is fixed at boot: the kernel trusts the module named `init`, and `init` decides who gets what.
* Capabilities cannot be revoked, and the table does not remember where a delegated capability came from.
* The table has a fixed size.
* There is no capability for IPC: any process may send a message to any process id.
* A capability over ports that drive DMA (disk controllers, the PCI configuration ports) lets its owner write anywhere in physical memory, since there is no IOMMU. It is as strong as root and should be granted accordingly.
