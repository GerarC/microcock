# Architecture (x86)

The code under `cock/arch/x86`. Mechanism lives here and policy lives in the core: this layer reports events and the core decides what to do with them. The core reaches it only through the `<hal/...>` headers (see the HAL section of [project structure](../project_structure.md)).

| Page | Topic |
|------|-------|
| [bootloader.md](bootloader.md) | Multiboot, the entry stub, boot page tables, the early window |
| [gdt.md](gdt.md) | GDT, TSS, I/O permission bitmap, fault tasks |
| [idt.md](idt.md) | IDT gates, ISR and IRQ stubs, PIC |

Memory layout, the PMM and paging are described in [memory management](../core/memory_management.md) because their policy sits in the core.
