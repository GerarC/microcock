#ifndef ARCH_X86_PIC_HPP
#define ARCH_X86_PIC_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

constexpr int CASCADE_IRQ = 2;

/**
 * @brief Master Programmable Interrupt Controller (PIC1) I/O ports.
 */
enum class PIC1 : uint16_t {
	PORT = 0x20,
	COMMAND = PIC1::PORT,
	DATA = PIC1::PORT + 1
};

/**
 * @brief Slave Programmable Interrupt Controller (PIC2) I/O ports.
 */
enum class PIC2 : uint16_t {
	PORT = 0xA0,
	COMMAND = PIC2::PORT,
	DATA = PIC2::PORT + 1
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
 * @param master_offset Vector offset for the Master PIC (usually 0x20).
 * @param slave_offset Vector offset for the Slave PIC (usually 0x28).
 */
void PIC_remap(uint8_t master_offset, uint8_t slave_offset);

}

#endif // !PIC_HPP
