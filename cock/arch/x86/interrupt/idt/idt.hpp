#ifndef ARCH_X86_IDT_HPP
#define ARCH_X86_IDT_HPP
#include <cock/arch/x86/interrupt/irq/irq.hpp>
#include <cock/arch/x86/interrupt/isr/isr.hpp>
#include <stdint.h>

namespace cock::arch::x86 {

/**
 * @brief Manager class for the Interrupt Descriptor Table (IDT).
 * * Binds CPU exceptions, hardware interrupts, and system call vectors to
 * handler functions.
 */
class IDT {
  public:
	/**
	 * @brief Hardware-defined structure for an IDT gate entry.
	 */
	typedef struct __attribute__((packed)) _idt_entry_t {
		uint16_t base_low;	/**< Lower 16 bits of the handler's address. */
		uint16_t selector;	/**< Kernel code segment selector. */
		uint8_t reserved;	/**< Always zero. */
		uint8_t flags;		/**< Type and attributes (e.g., Ring 0 vs Ring 3
							   accessibility). */
		uint16_t base_high; /**< Higher 16 bits of the handler's address. */
	} Entry;

	/**
	 * @brief Pointer structure used to load the IDT via the `lidt` instruction.
	 */
	typedef struct __attribute__((packed)) _idt_descriptor_t {
		uint16_t limit;
		uintptr_t base;
	} Descriptor;

  public:
	IDT();

	/**
	 * @brief Initializes the IDT, remaps the PIC, and installs default ISR/IRQ
	 * stubs.
	 */
	static void init();

  private:
	static constexpr int MAX_ENTRIES = 256;
	static __attribute__((aligned(0x10))) Entry entries[MAX_ENTRIES];
	static Descriptor descriptor;

	/**
	 * @brief Configures a specific IDT gate.
	 * @param num The interrupt vector index (0-255).
	 * @param base The virtual address of the handler function.
	 * @param selector The GDT segment selector to use.
	 * @param flags The gate type and privilege level.
	 */
	static void setGate(uint32_t num, uint32_t base, uint32_t selector,
						uint8_t flags);
};

} // namespace cock::arch::x86

#endif // !IDT_HPP
