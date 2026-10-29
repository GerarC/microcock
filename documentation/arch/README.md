# Architecture (x86)

The code under `cock/arch/x86`. Mechanism lives here and policy lives in the core: this layer reports events and the core decides what to do with them. The core reaches it only through the `<hal/...>` headers (see [the HAL](../core/hal.md)).

## boot/

| Page | Topic |
|------|-------|
| [bootloader.md](boot/bootloader.md) | Multiboot, the entry stub, boot page tables, the early window |
| [module_list.md](boot/module_list.md) | From the Multiboot module list to the `BootModule` array |

## cpu/

| Page | Topic |
|------|-------|
| [gdt.md](cpu/gdt.md) | GDT, TSS, I/O permission bitmap, fault tasks |

## interrupts/

| Page | Topic |
|------|-------|
| [idt.md](interrupts/idt.md) | IDT gates and the interrupt stubs |
| [exceptions.md](interrupts/exceptions.md) | CPU exceptions: what is reported and what the core does |
| [irq.md](interrupts/irq.md) | Hardware interrupt dispatch and handler installation |
| [syscall_entry.md](interrupts/syscall_entry.md) | `int 0x80`, register convention, the syscall table |

## devices/

| Page | Topic |
|------|-------|
| [pic.md](devices/pic.md) | The two 8259A controllers: remap, masks, end of interrupt, spurious lines |
| [timer.md](devices/timer.md) | The PIT that drives the scheduler tick |

Memory layout, the PMM and paging are described in [memory management](../core/memory/README.md) because their policy sits in the core.
