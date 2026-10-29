#include <cock/arch/x86/gdt/segments.hpp>
#include <cock/arch/x86/interrupt/idt/idt.hpp>
#include <cock/utils/logger.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void *irq_stub_table[];
extern "C" void *isr_stub_table[];
extern "C" void *isr_stub_128;

extern "C" void idt_flush(uintptr_t);
namespace cock::arch::x86 {

using cock::utils::Logger;

constexpr uint8_t KERNEL_INTERRUPT_GATE = 0x8E;
constexpr uint8_t USER_INTERRUPT_GATE = 0xEE;
constexpr uint8_t TASK_GATE = 0x85;
constexpr uint32_t SYSCALL_VECTOR = 128;
constexpr uint32_t NMI_VECTOR = 2;
constexpr uint32_t DOUBLE_FAULT_VECTOR = 8;
constexpr uint32_t TASK_GATE_OFFSET = 0;

IDT::IDT() {}

__attribute__((aligned(0x10))) IDT::Entry IDT::entries[MAX_ENTRIES];
IDT::Descriptor IDT::descriptor;

void IDT::init() {
	size_t entries_size = sizeof(Entry) * MAX_ENTRIES;
	descriptor.limit = entries_size - 1;
	descriptor.base = reinterpret_cast<uintptr_t>(&entries);
	memset(&entries, 0, entries_size);

	for (size_t isr = 0; isr < ISR_NUM; isr++)
		setGate(isr, reinterpret_cast<uint32_t>(isr_stub_table[isr]),
				KERNEL_CODE_SELECTOR, KERNEL_INTERRUPT_GATE);

	setGate(NMI_VECTOR, TASK_GATE_OFFSET, NMI_TSS_SELECTOR, TASK_GATE);
	setGate(DOUBLE_FAULT_VECTOR, TASK_GATE_OFFSET, DOUBLE_FAULT_TSS_SELECTOR,
			TASK_GATE);

	for (size_t irq = 0; irq < IRQ_NUM; irq++)
		setGate(ISR_NUM + irq, reinterpret_cast<uint32_t>(irq_stub_table[irq]),
				KERNEL_CODE_SELECTOR, KERNEL_INTERRUPT_GATE);
	setGate(SYSCALL_VECTOR, reinterpret_cast<uint32_t>(&isr_stub_128),
			KERNEL_CODE_SELECTOR, USER_INTERRUPT_GATE);

	idt_flush(reinterpret_cast<uintptr_t>(&descriptor));
	Logger::debug("IDT Initialized");
}

void IDT::setGate(uint32_t num, uint32_t base, uint32_t selector,
				  uint8_t flags) {
	entries[num].base_low = base & 0xFFFF;
	entries[num].base_high = (base >> 0x10) & 0xFFFF;
	entries[num].selector = selector;
	entries[num].reserved = 0;
	entries[num].flags = flags;
}

} // namespace cock::arch::x86
