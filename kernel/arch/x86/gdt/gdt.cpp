#include "gdt.hpp"

typedef long unsigned int addr_t;

extern "C" void gdt_flush(uintptr_t); // flush

namespace cock::arch::x86 {

GDT::GDT(){}

void GDT::init() {
	pointer.limit = (sizeof(Entry) * MAX_ENTRIES - 1);
	pointer.base = reinterpret_cast<uintptr_t>(&entries);

	setGate(0, 0, 0, 0, 0);				// NULL segment
	setGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment
	setGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment
	setGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code Segment
	setGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data Segment

	gdt_flush(reinterpret_cast<uintptr_t>(&pointer));
}

void GDT::setGate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access,
				  uint8_t gran) {
	entries[num].base_low = base & 0xFFFF;
	entries[num].base_middle = (base >> 0x10) & 0xFF;
	entries[num].base_high = (base >> 0x18) & 0xFF;

	entries[num].limit = (limit & 0xFFFF);
	entries[num].flags = (limit >> 0x10) & 0x0F;
	entries[num].flags |= gran & 0xF0;

	entries[num].access = access;
}

} // namespace arch::x86
