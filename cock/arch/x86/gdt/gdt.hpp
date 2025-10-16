#ifndef GDT_HPP
#define GDT_HPP
#include <stdint.h>

namespace cock::arch::x86 {
typedef struct __attribute__((packed)) _gdt_entry_t {
	uint16_t limit;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t flags;
	uint8_t base_high;
} Entry;

typedef struct __attribute__((packed)) _gdt_pointer_t {
	uint16_t limit;
	uintptr_t base;
} Pointer;

typedef struct __attribute__((packed)) _task_state_segement_entry {
	uint32_t prev_tss;
	uint32_t esp0;
	uint32_t ss0;
	uint32_t esp1;
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;
	uint32_t cs;
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;
	uint32_t trap;
	uint32_t iomap_base;
} TssEntry;

class GDT {
  public:
	GDT();
	void init();

  private:
	static constexpr int MAX_ENTRIES = 6;
	Entry entries[MAX_ENTRIES];
    TssEntry tss_entry;
	Pointer pointer;

	void setGate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access,
				 uint8_t gran);
	void writeTSS(uint32_t num, uint16_t ss0, uint32_t esp0);
};

} // namespace cock::arch::x86

#endif // !GDT_HPP
