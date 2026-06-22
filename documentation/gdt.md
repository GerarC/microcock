# Global Descriptor Table (GDT)

The Global Descriptor Table defines the memory segments for the x86 architecture and is managed by the `GDT` class in `cock/arch/x86/gdt/gdt.cpp`.

## Initialization
`GDT::init()` sets up a flat memory model by configuring 6 standard descriptor entries and the Task State Segment (TSS).

```cpp
void GDT::init() {
    pointer.limit = (sizeof(Entry) * MAX_ENTRIES - 1);
    pointer.base = reinterpret_cast<uintptr_t>(&entries);

    GDT::setGate(0, 0, 0, 0, 0);                // NULL segment
    GDT::setGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment
    GDT::setGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment
    GDT::setGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code Segment
    GDT::setGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data Segment
    GDT::writeTSS(5, 0x10, 0x0);

    gdt_flush(reinterpret_cast<uintptr_t>(&pointer));
    tss_flush();
}
```

## Task State Segment (TSS)
A TSS is written to manage hardware context switching. It includes an I/O Permission Map of size 8193 to securely control thread access to hardware `in/out` ports.

```cpp
void GDT::writeTSS(uint32_t num, uint16_t ss0, uint32_t esp0) {
    size_t tts_size = sizeof(tssEntry);
    uint32_t base = reinterpret_cast<uint32_t>(&tssEntry);
    uint32_t limit = base + tts_size - 1;

    GDT::setGate(num, base, limit, 0xE9, 0x00);
    memset(&tssEntry, 0, tts_size);

    tssEntry.ss0 = ss0;
    tssEntry.esp0 = esp0;
    tssEntry.cs = 0x08;
    tssEntry.ss = 0x10;
    // ...
    tssEntry.iomap_base = IO_MAP_BASE;
    memset(tssEntry.io_permission_map, 0xFF, sizeof(tssEntry.io_permission_map));
}
```
