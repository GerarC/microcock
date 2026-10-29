# PIC (8259A)

Two cascaded 8259A Programmable Interrupt Controllers carry the 16 hardware interrupt lines (`cock/arch/x86/interrupt/pic/pic.cpp`). The master serves lines 0 to 7 and the slave serves lines 8 to 15 through input 2 of the master (`CASCADE_IRQ`). The driver runs in Ring 0 with `in` and `out`.

## Remap

By default the PIC maps lines 0 to 7 to vectors `0x08` to `0x0F`, which collide with CPU exceptions such as the double fault. `PIC_remap(master_offset, slave_offset)` runs the ICW1 to ICW4 sequence on both chips (cascade on input 2, 8086 mode) with a short I/O delay between writes, and then **masks every line**. The kernel calls it with `0x20` and `0x28` before the IDT is loaded (see [idt](../interrupts/idt.md#idt-initialization)), with interrupts off. Lines are unmasked one by one afterwards, as a handler is installed.

## Operations

| Function | What it does |
|----------|--------------|
| `PIC_mask(line)` / `PIC_unmask(line)` | Read-modify-write of the mask register of the chip that owns the line. Unmasking a line of the slave also unmasks the cascade input on the master |
| `PIC_end_of_interrupt(line)` | Sends the end of interrupt to the slave first when the line is 8 to 15, and always to the master. It must be sent once per real interrupt or the line stays blocked |
| `PIC_read_isr(slave)` | Reads the in-service register of one chip; used to recognise spurious interrupts |

`PIC_mask` and `PIC_unmask` are also what the core reaches through `hal::irq_mask` and `hal::irq_unmask` when it masks the line of a driver until the driver acknowledges (see [ipc](../../core/ipc.md#interrupts-as-messages)).

## Spurious interrupts

The chip can raise line 7 (master) or line 15 (slave) with no device behind it. The [irq](../interrupts/irq.md) dispatcher asks the in-service register: if bit 7 is clear the interrupt is spurious and is ignored. A spurious line 15 still needs an end of interrupt on the master for the cascade line, and the dispatcher sends it.
