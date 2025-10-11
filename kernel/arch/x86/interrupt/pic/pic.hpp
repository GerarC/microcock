#ifndef PIC_HPP
#define PIC_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

constexpr int CASCADE_IRQ = 2;

/** Master PIC
 * */
enum class PIC1 : uint16_t {
	PORT = 0x20,
	COMMAND = PIC1::PORT,
	DATA = PIC1::PORT + 1
};

/** Slave PIC
 * */
enum class PIC2 : uint16_t {
	PORT = 0xA0,
	COMMAND = PIC2::PORT,
	DATA = PIC2::PORT + 1
};

enum class ICW1 : uint8_t {
	ICW4 = 0x01, // ICW4 will be present
	SINGLE = 0x02, // Single (cascade) mode
	INTERVAL4 = 0x04, // calls address interval 4
	LEVEL = 0x08, // Level triggered (edge) mode
	INIT = 0x10, // initialization
};

enum class ICW4 : uint8_t {
	EES = 0x01, // 8086 mode
	AUTO = 0x02, // Auto End Of Interruption
	BUF_SLAVE = 0x04, // Buffered mode - slave
	BUF_MASTER = 0x08, // Buffered mode - master
	SFNM = 0x10, // Special fully nested (not)
};


void PIC_remap(uint8_t master_offset, uint8_t slave_offset);

}

#endif // !PIC_HPP
