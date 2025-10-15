#include "gdt.hpp"
#include <stddef.h>
#include <string.h>

typedef long unsigned int addr_t;

extern "C" void gdt_flush(uintptr_t); // flush
extern "C" void tss_flush(); // flush

namespace cock::arch::x86 {

GDT::GDT() {}

void GDT::init() {
	pointer.limit = (sizeof(Entry) * MAX_ENTRIES - 1);
	pointer.base = reinterpret_cast<uintptr_t>(&entries);

	setGate(0, 0, 0, 0, 0);				   // NULL segment
	setGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment
	setGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment
	setGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code Segment
	setGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data Segment
	writeTSS(5, 0x10, 0x0);

	gdt_flush(reinterpret_cast<uintptr_t>(&pointer));
    tss_flush();
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

void GDT::writeTSS(uint32_t num, uint16_t ss0, uint32_t esp0) {
    size_t tts_size = sizeof(tss_entry);
	uint32_t base = reinterpret_cast<uint32_t>(&tss_entry);
	uint32_t limit = base + tts_size;

    setGate(num, base, limit, 0xE9, 0x00);
    memset(&tss_entry, 0, tts_size);
    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;

    tss_entry.cs = 0x08 | 0x3;

    tss_entry.ss = 0x10 | 0x3;
    tss_entry.es = 0x10 | 0x3;
    tss_entry.ds = 0x10 | 0x3;
    tss_entry.fs = 0x10 | 0x3;
    tss_entry.gs = 0x10 | 0x3;

}

} // namespace cock::arch::x86
