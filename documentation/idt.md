# Interrupt Descriptor Table (IDT)

The Interrupt Descriptor Table binds CPU exceptions, hardware interrupts, and system call vectors to their handler functions.

## IDT Initialization
The `IDT::init()` method initializes 256 entries, remaps the Programmable Interrupt Controller (PIC), and registers Interrupt Service Routines (ISRs) and Interrupt Requests (IRQs).

```cpp
void IDT::init() {
    size_t entries_size = sizeof(Entry) * MAX_ENTRIES;
    descriptor.limit = entries_size - 1;
    descriptor.base = reinterpret_cast<uintptr_t>(&entries);
    memset(&entries, 0, entries_size);

    PIC_remap(0x20, 0x28); // Remap Master to 0x20, Slave to 0x28

    for (size_t isr = 0; isr < ISR_NUM; isr++)
        setGate(isr, reinterpret_cast<uint32_t>(isr_stub_table[isr]), 0x08, 0x8E);

    for (size_t irq = 0; irq < IRQ_NUM; irq++)
        setGate(ISR_NUM + irq, reinterpret_cast<uint32_t>(irq_stub_table[irq]), 0x08, 0x8E);
    
    setGate(128, reinterpret_cast<uint32_t>(&isr_stub_128), 0x08, 0xEE); // Syscalls
    
    idt_flush(reinterpret_cast<uintptr_t>(&descriptor));
}
```

## System Calls (ISRs)
System calls are mapped to the `0x80` vector (128 in decimal). The `isr_handler` intercepts these calls and forwards them to the `SyscallManager`.

```cpp
extern "C" uintptr_t isr_handler(uintptr_t current_esp) {
    InterruptRegisters *regs = reinterpret_cast<InterruptRegisters *>(current_esp);

    if (regs->int_no == SYSCALL_CODE) {
        uint32_t syscall_num = regs->eax;
        // ... gather arguments ...
        SystemCall handler = SyscallManager::SyscallManager::getSyscall(syscall_num);

        if (handler == nullptr) {
            regs->eax = (uint32_t)-1;
            return current_esp;
        }

        uintptr_t result = handler(arg0, arg1, arg2, arg3, arg4, arg5);
        // ...
        regs->eax = static_cast<uint32_t>(result);
        return current_esp;
    }
    // ... handles exceptions ...
}
```

## Hardware Interrupts (IRQs)
Hardware interrupts are handled by `irq_handler` in `cock/arch/x86/interrupt/irq/irq.cpp`. Critically, if the timer (`IRQ0`) fires, it invokes the Scheduler to swap contexts.

```cpp
extern "C" uintptr_t irq_handler(uintptr_t current_esp) {
    InterruptRegisters *regs = reinterpret_cast<InterruptRegisters *>(current_esp);
    size_t handler_index = regs->int_no - BASE_ROUTINE_INDEX;

    if (handler_index < IRQ_NUM) {
        IrqRoutine handler = irq_routines[handler_index];
        if (handler) handler(regs);
        
        if (regs->int_no >= 40) out_port_b((uint16_t)PIC2::COMMAND, 0x20);
        out_port_b((uint16_t)PIC1::COMMAND, 0x20);
    }

    // Context switch if IRQ0 fired
    if (regs->int_no == TIMER_NO)
        return cock::core::task::Scheduler::schedule(current_esp);
        
    return current_esp;
}
```
