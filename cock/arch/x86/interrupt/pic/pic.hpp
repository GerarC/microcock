#ifndef ARCH_X86_PIC_HPP
#define ARCH_X86_PIC_HPP

/**
 * @file
 * @brief Driver for the pair of cascaded 8259A Programmable Interrupt
 * Controllers.
 *
 * The master PIC serves IRQ 0-7 and the slave serves IRQ 8-15 through the
 * master's input 2. All functions talk to the chips through `in` and `out`
 * and run in Ring 0.
 */

#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Master PIC input wired to the slave PIC. */
constexpr int CASCADE_IRQ = 2;
/** @brief Number of IRQ inputs of one PIC chip. */
constexpr uint8_t PIC_IRQS_PER_CHIP = 8;
/** @brief OCW2 command: non specific end of interrupt. */
constexpr uint8_t PIC_END_OF_INTERRUPT = 0x20;
/** @brief OCW3 command: next read of the command port returns the in-service
 * register. */
constexpr uint8_t PIC_READ_ISR = 0x0B;
/** @brief Mask register value that disables all eight lines of a chip. */
constexpr uint8_t PIC_MASK_ALL = 0xFF;

/**
 * @brief Master Programmable Interrupt Controller (PIC1) I/O ports.
 */
enum class PIC1 : uint16_t {
	PORT = 0x20,			 /**< Base I/O port of the master PIC. */
	COMMAND = PIC1::PORT,	 /**< Command register (write) and status (read). */
	DATA = PIC1::PORT + 1	 /**< Data register: initialization words and the
								interrupt mask. */
};

/**
 * @brief Slave Programmable Interrupt Controller (PIC2) I/O ports.
 */
enum class PIC2 : uint16_t {
	PORT = 0xA0,			 /**< Base I/O port of the slave PIC. */
	COMMAND = PIC2::PORT,	 /**< Command register (write) and status (read). */
	DATA = PIC2::PORT + 1	 /**< Data register: initialization words and the
								interrupt mask. */
};

/**
 * @brief Initialization Command Word 1 (ICW1) flags.
 */
enum class ICW1 : uint8_t {
	ICW4 = 0x01,      /**< ICW4 will be present. */
	SINGLE = 0x02,    /**< Single (cascade) mode. */
	INTERVAL4 = 0x04, /**< Call address interval 4. */
	LEVEL = 0x08,     /**< Level triggered (edge) mode. */
	INIT = 0x10,      /**< Initialization sequence trigger. */
};

/**
 * @brief Initialization Command Word 4 (ICW4) flags.
 */
enum class ICW4 : uint8_t {
	EES = 0x01,       /**< 8086/88 (x86) mode. */
	AUTO = 0x02,      /**< Auto End Of Interruption (AEOI). */
	BUF_SLAVE = 0x04, /**< Buffered mode - slave. */
	BUF_MASTER = 0x08,/**< Buffered mode - master. */
	SFNM = 0x10,      /**< Special fully nested mode. */
};

/**
 * @brief Remaps the PIC to avoid conflicts with standard CPU exceptions.
 * * By default, the PIC maps IRQ 0-7 to vectors 0x08-0x0F, which clash with x86 exceptions (like Double Fault).
 *
 * Runs the ICW1 to ICW4 initialization sequence on both chips (cascade on
 * input 2, 8086 mode) with a short I/O delay between writes, then masks every
 * line. Lines must be unmasked one by one afterwards. Call with interrupts
 * disabled.
 *
 * @param master_offset Vector offset for the Master PIC (usually 0x20).
 * @param slave_offset Vector offset for the Slave PIC (usually 0x28).
 */
void PIC_remap(uint8_t master_offset, uint8_t slave_offset);

/**
 * @brief Masks (disables) one IRQ line.
 *
 * Read-modify-write of the mask register of the chip that owns the line.
 *
 * @param irq IRQ line, 0 to 15.
 */
void PIC_mask(uint8_t irq);

/**
 * @brief Unmasks (enables) one IRQ line.
 *
 * Read-modify-write of the owning chip's mask register. For lines 8 to 15 it
 * also unmasks the cascade input on the master so the slave can interrupt.
 *
 * @param irq IRQ line, 0 to 15.
 */
void PIC_unmask(uint8_t irq);

/**
 * @brief Reads the in-service register (ISR) of one chip.
 *
 * Sends the OCW3 read-ISR command and reads the result. A set bit means the
 * chip is currently servicing that input; used to detect spurious IRQ 7 and
 * 15.
 *
 * @param slave True to read the slave PIC, false for the master.
 * @return Bit mask of the in-service inputs of that chip.
 */
uint8_t PIC_read_isr(bool slave);

/**
 * @brief Signals the end of an interrupt.
 *
 * Writes the end of interrupt command to the slave first when the line belongs
 * to it (8 to 15), and always to the master. Must be sent once per real
 * interrupt or the line stays blocked.
 *
 * @param irq IRQ line that was serviced, 0 to 15.
 */
void PIC_end_of_interrupt(uint8_t irq);

}

#endif // !PIC_HPP
