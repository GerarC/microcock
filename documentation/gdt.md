# Global Descriptor Table (GDT)

The Global Descriptor Table defines the memory segments for the x86 architecture and is managed by the `GDT` class in `cock/arch/x86/gdt/gdt.cpp`.

## Initialization
`GDT::init()` sets up a flat memory model by configuring 5 standard descriptor entries and the Task State Segment (TSS) in a table of 6.

```cpp
void GDT::init() {
	pointer.limit = (sizeof(Entry) * MAX_ENTRIES - 1);
	pointer.base = reinterpret_cast<uintptr_t>(&entries);

	GDT::setGate(0, 0, 0, 0, 0);
	GDT::setGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
	GDT::setGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
	GDT::setGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
	GDT::setGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
	GDT::writeTSS(5, KERNEL_DATA_SELECTOR, 0x0);

	gdt_flush(reinterpret_cast<uintptr_t>(&pointer));
	tss_flush();
}
```

| Index | Descriptor |
|-------|------------|
| 0 | Null |
| 1 | Kernel code (ring 0) |
| 2 | Kernel data (ring 0) |
| 3 | User code (ring 3) |
| 4 | User data (ring 3) |
| 5 | TSS |

The selectors used by the rest of the kernel are named in `cock/arch/x86/gdt/segments.hpp`.

## Task State Segment (TSS)
The kernel does not switch tasks with the hardware. The TSS exists for two things: the stack the CPU switches to when an interrupt arrives from Ring 3 (`ss0` and `esp0`, updated at every context switch), and the I/O Permission Map, of size 8193 bytes, that controls which `in` and `out` ports Ring 3 code may use.

```cpp
void GDT::writeTSS(uint32_t num, uint16_t ss0, uint32_t esp0) {
	size_t tss_size = sizeof(tssEntry);
	uint32_t base = reinterpret_cast<uint32_t>(&tssEntry);
	uint32_t limit = tss_size - 1;

	GDT::setGate(num, base, limit, TSS_ACCESS, TSS_GRANULARITY);
	memset(&tssEntry, 0, tss_size);

	tssEntry.ss0 = ss0;
	tssEntry.esp0 = esp0;

	tssEntry.iomap_base = IO_MAP_BASE;
	memset(tssEntry.io_permission_map, IO_DENIED, sizeof(tssEntry.io_permission_map));
}
```

The limit of the descriptor is the size of the structure minus one, and its access byte (`0x89`) gives it privilege level 0, so Ring 3 cannot reference it. Every port starts denied.

### Applying a process context
Each process keeps its own bitmap inside its `ArchProcessContext`, along with a version number. `GDT::applyIoPermissions` runs when the scheduler switches to a thread of a user process:

```cpp
void GDT::applyIoPermissions(const uint8_t *map, uint32_t bytes, uint32_t version) {
	if (appliedVersion == version) return;
	appliedVersion = version;

	if (bytes == 0) {
		tssEntry.iomap_base = IO_MAP_DISABLED;
		return;
	}

	memcpy(tssEntry.io_permission_map, map, bytes);
	if (dirtyBytes > bytes)
		memset(tssEntry.io_permission_map + bytes, IO_DENIED, dirtyBytes - bytes);
	dirtyBytes = bytes;
	tssEntry.iomap_base = IO_MAP_BASE;
}
```

* If the TSS already holds that version, nothing is copied.
* Only the bytes the process actually uses are copied, and the bytes left over from the previous process are denied again.
* A process with no ports points `iomap_base` past the limit of the TSS, which denies every port without copying a single byte.

Ports reach a process only through `SYS_REQUEST_RESOURCE`, which first checks the capability (see [capabilities](capabilities.md)) and refuses the ports of the PIC and the PIT, which belong to the kernel.
