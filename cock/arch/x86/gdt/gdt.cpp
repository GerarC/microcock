#include <cock/utils/logger.hpp>
#include <cock/arch/x86/gdt/gdt.hpp>
#include <stddef.h>
#include <string.h>

typedef long unsigned int addr_t;

extern "C" void gdt_flush(uintptr_t); // gdt flush
extern "C" void tss_flush();		  // tss flush

namespace cock::arch::x86 {

using cock::utils::Logger;

Entry GDT::entries[MAX_ENTRIES];
TssEntry GDT::tssEntry;
Pointer GDT::pointer;

GDT::GDT() {}

void GDT::init() {
	pointer.limit = (sizeof(Entry) * MAX_ENTRIES - 1);
	pointer.base = reinterpret_cast<uintptr_t>(&entries);

	GDT::setGate(0, 0, 0, 0, 0);				// NULL segment
	GDT::setGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment
	GDT::setGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment
	GDT::setGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code Segment
	GDT::setGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data Segment
	GDT::writeTSS(5, 0x10, 0x0);

	gdt_flush(reinterpret_cast<uintptr_t>(&pointer));
	tss_flush();

	Logger::debug("GTD Initialized");
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
	size_t tts_size = sizeof(tssEntry);
	uint32_t base = reinterpret_cast<uint32_t>(&tssEntry);
	uint32_t limit = base + tts_size;

	GDT::setGate(num, base, limit, 0xE9, 0x00);
	memset(&tssEntry, 0, tts_size);
	tssEntry.ss0 = ss0;
	tssEntry.esp0 = esp0;

	tssEntry.cs = 0x08 | 0x3;

	tssEntry.ss = 0x10 | 0x3;
	tssEntry.es = 0x10 | 0x3;
	tssEntry.ds = 0x10 | 0x3;
	tssEntry.fs = 0x10 | 0x3;
	tssEntry.gs = 0x10 | 0x3;
}


void GDT::setKernelStack(uint32_t stack_pointer){
    tssEntry.esp0 = stack_pointer;

}


} // namespace cock::arch::x86
