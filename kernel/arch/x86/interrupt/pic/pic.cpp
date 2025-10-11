#include "pic.hpp"

#include "../utils/functs.hpp"

namespace cock::arch::x86 {

using utils::out_port_b;

void PIC_remap(uint8_t master_offset, uint8_t slave_offset) {
	out_port_b((uint16_t)PIC1::COMMAND, static_cast<uint8_t>(ICW1::INIT) |
											static_cast<uint8_t>(ICW1::ICW4));
	out_port_b((uint16_t)PIC2::COMMAND, static_cast<uint8_t>(ICW1::INIT) |
											static_cast<uint8_t>(ICW1::ICW4));

	out_port_b((uint16_t)PIC1::DATA, master_offset);
	out_port_b((uint16_t)PIC2::DATA, slave_offset);

	out_port_b((uint16_t)PIC1::DATA, 1 << CASCADE_IRQ);
	out_port_b((uint16_t)PIC2::DATA, 0x02);

	out_port_b((uint16_t)PIC1::DATA, static_cast<uint8_t>(ICW4::EES));
	out_port_b((uint16_t)PIC2::DATA, static_cast<uint8_t>(ICW4::EES));

	// unmask both PICs
	out_port_b((uint16_t)PIC1::DATA, 0x0);
	out_port_b((uint16_t)PIC2::DATA, 0x0);
}

}
