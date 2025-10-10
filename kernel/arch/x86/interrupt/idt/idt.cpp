#include "idt.hpp"
#include "../pic/pic.hpp"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

constexpr size_t ISR_NUM = 32;
constexpr size_t IRQ_NUM = 16;
extern "C" void *irq_stub_table[];
extern "C" void *isr_stub_table[];
extern "C" void *isr_stub_128;
extern "C" void *isr_stub_177;

extern "C" void idt_flush(uintptr_t);
namespace arch::x86 {


/** Initializing PICs
 * */

IDT::IDT() {}

void IDT::init() {
	size_t entries_size = sizeof(Entry) * MAX_ENTRIES;
	descriptor.limit = entries_size - 1;
	descriptor.base = reinterpret_cast<uintptr_t>(&entries);
	memset(&entries, 0, entries_size);

	PIC_remap(0x20, 0x28);

	for (size_t isr = 0; isr < ISR_NUM; isr++)
		setGate(isr,
				reinterpret_cast<uint32_t>(isr_stub_table[isr]), 0x08,
				0x8E);

    for(size_t irq = 0; irq < IRQ_NUM; irq++ )
		setGate(ISR_NUM + irq,
				reinterpret_cast<uint32_t>(irq_stub_table[irq]), 0x08,
				0x8E);
	setGate(128, reinterpret_cast<uint32_t>(&isr_stub_128), 0x08, 0x8E);
	setGate(177, reinterpret_cast<uint32_t>(&isr_stub_177), 0x08, 0x8E);

	idt_flush(reinterpret_cast<uintptr_t>(&descriptor));
}

void IDT::setGate(uint32_t num, uint32_t base, uint32_t selector,
				  uint8_t flags) {
	entries[num].base_low = base & 0xFFFF;
	entries[num].base_high = (base >> 0x10) & 0xFFFF;
	entries[num].selector = selector;
	entries[num].reserved = 0;
	entries[num].flags = flags | 0x60;
}

} // namespace arch::x86
