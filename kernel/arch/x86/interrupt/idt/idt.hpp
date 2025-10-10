#ifndef IDT_HPP
#define IDT_HPP
#include <stdint.h>

namespace arch::x86 {
class IDT {
  public:
	typedef struct __attribute__((packed)) _idt_entry_t {
		uint16_t base_low;
		uint16_t selector;
		uint8_t reserved;
		uint8_t flags;
		uint16_t base_high;
	} Entry;

	typedef struct __attribute__((packed)) _idt_descriptor_t {
		uint16_t limit;
		uintptr_t base;
	} Descriptor;

  public:
	IDT();
	void init();

  private:
	static constexpr int MAX_ENTRIES = 256;
	__attribute__((aligned(0x10))) Entry entries[MAX_ENTRIES];
	Descriptor descriptor;

	void setGate(uint32_t num, uint32_t base, uint32_t selector, uint8_t flags);
};

} // namespace arch::x86

#endif // !IDT_HPP
