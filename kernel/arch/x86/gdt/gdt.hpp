#ifndef GDT_HPP
#define GDT_HPP
#include <stdint.h>

namespace cock::arch::x86 {
class GDT {
  public:
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

  public:
	GDT();
	void init();

  private:
	static constexpr int MAX_ENTRIES = 5;
	Entry entries[MAX_ENTRIES];
	Pointer pointer;

	void setGate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access,
				 uint8_t gran);
};

} // namespace arch::x86

#endif // !GDT_HPP
