#include <cock/core/gdt.hpp>

extern "C" void gdt_flush(GdtPointer *); // flush

GdtEntry gdt_entries[GDT_ITEM_NUM];
GdtPointer gdt_ptr;

void init_gdt() {
	gdt_ptr.limit = (sizeof(GdtEntry) * GDT_ITEM_NUM - 1);
	gdt_ptr.base = gdt_entries;

	set_gdt_gate(0, 0, 0, 0, 0);				  // NULL segment
	set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment
	set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment
	set_gdt_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code Segment
	set_gdt_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data Segment


	gdt_flush(&gdt_ptr);
}

void set_gdt_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access,
				uint8_t gran) {
	gdt_entries[num].base_low = base & 0xFFFF;
	gdt_entries[num].base_middle = (base >> 16) & 0xFF;
	gdt_entries[num].base_high = (base >> 24) & 0xFF;

	gdt_entries[num].limit = (limit & 0xFFFF);
	gdt_entries[num].flags = (limit >> 16) & 0x0F;
	gdt_entries[num].flags |= gran & 0xF0;

    gdt_entries[num].access = access;
}
