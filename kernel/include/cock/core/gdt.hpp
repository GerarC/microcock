#ifndef GDT_HPP
#define GDT_HPP
#include <stdint.h>

#define GDT_ITEM_NUM 5

typedef struct _gdt_entry_t {
	uint16_t limit;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t flags;
	uint8_t base_high;
} __attribute__((packed)) GdtEntry;

typedef struct _gdt_pointer_t {
	uint16_t limit;
	unsigned int base;
} __attribute__((packed)) GdtPointer;

void init_gdt();
void set_gdt_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access,
				uint8_t gran);

#endif // !GDT_HPP

