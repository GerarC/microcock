#ifndef ARCH_X86_IDT_HPP
#define ARCH_X86_IDT_HPP

/**
 * @file
 * @brief Interrupt Descriptor Table: vector to handler binding for i386.
 *
 * Vector layout: 0..31 CPU exceptions (2 and 8 are task gates onto private
 * stacks), 32..47 the remapped PIC lines, 128 (0x80) the system call gate
 * reachable from Ring 3.
 */

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
		uint16_t limit; /**< Size of the table in bytes minus one. */
		uintptr_t base; /**< Linear address of the first gate. */
	} Descriptor;

  public:
	/** @brief Empty constructor; the table is built by init(). */
	IDT();

	/**
	 * @brief Initializes the IDT, remaps the PIC, and installs default ISR/IRQ
	 * stubs.
	 *
	 * Clears all 256 gates, installs interrupt gates for the 32 exception
	 * stubs and the 16 IRQ stubs (Ring 0 only), replaces vectors 2 (NMI) and 8
	 * (double fault) with task gates, installs vector 0x80 with DPL 3 so Ring 3
	 * can execute `int 0x80`, and loads the table with `lidt` (`idt_flush`).
	 * Requires the GDT to be loaded already because gates reference its
	 * selectors. Ring 0, once, during early boot with interrupts disabled.
	 */
	static void init();

  private:
	/** @brief Number of vectors the CPU can raise. */
	static constexpr int MAX_ENTRIES = 256;
	/** @brief The gate table, 16 byte aligned. */
	static __attribute__((aligned(0x10))) Entry entries[MAX_ENTRIES];
	/** @brief Operand of `lidt`. */
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
