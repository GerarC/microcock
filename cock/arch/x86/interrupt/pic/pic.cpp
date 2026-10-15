#include <cock/arch/x86/interrupt/pic/pic.hpp>
#include <cock/arch/x86/utils/helpers.hpp>

namespace cock::arch::x86 {

using utils::in_port_b;
using utils::io_wait;
using utils::out_port_b;

static uint16_t data_port(uint8_t irq) {
	if (irq < PIC_IRQS_PER_CHIP) return static_cast<uint16_t>(PIC1::DATA);
	return static_cast<uint16_t>(PIC2::DATA);
}

void PIC_remap(uint8_t master_offset, uint8_t slave_offset) {
	out_port_b((uint16_t)PIC1::COMMAND, static_cast<uint8_t>(ICW1::INIT) |
											static_cast<uint8_t>(ICW1::ICW4));
	io_wait();
	out_port_b((uint16_t)PIC2::COMMAND, static_cast<uint8_t>(ICW1::INIT) |
											static_cast<uint8_t>(ICW1::ICW4));
	io_wait();

	out_port_b((uint16_t)PIC1::DATA, master_offset);
	io_wait();
	out_port_b((uint16_t)PIC2::DATA, slave_offset);
	io_wait();

	out_port_b((uint16_t)PIC1::DATA, 1 << CASCADE_IRQ);
	io_wait();
	out_port_b((uint16_t)PIC2::DATA, CASCADE_IRQ);
	io_wait();

	out_port_b((uint16_t)PIC1::DATA, static_cast<uint8_t>(ICW4::EES));
	io_wait();
	out_port_b((uint16_t)PIC2::DATA, static_cast<uint8_t>(ICW4::EES));
	io_wait();

	out_port_b((uint16_t)PIC1::DATA, PIC_MASK_ALL);
	out_port_b((uint16_t)PIC2::DATA, PIC_MASK_ALL);
}

void PIC_mask(uint8_t irq) {
	uint16_t port = data_port(irq);
	uint8_t bit = irq % PIC_IRQS_PER_CHIP;
	out_port_b(port, in_port_b(port) | (1 << bit));
}

void PIC_unmask(uint8_t irq) {
	uint16_t port = data_port(irq);
	uint8_t bit = irq % PIC_IRQS_PER_CHIP;
	out_port_b(port, in_port_b(port) & ~(1 << bit));

	if (irq >= PIC_IRQS_PER_CHIP) PIC_unmask(CASCADE_IRQ);
}

uint8_t PIC_read_isr(bool slave) {
	uint16_t port = static_cast<uint16_t>(PIC1::COMMAND);
	if (slave) port = static_cast<uint16_t>(PIC2::COMMAND);
	out_port_b(port, PIC_READ_ISR);
	io_wait();
	return in_port_b(port);
}

void PIC_end_of_interrupt(uint8_t irq) {
	if (irq >= PIC_IRQS_PER_CHIP)
		out_port_b((uint16_t)PIC2::COMMAND, PIC_END_OF_INTERRUPT);
	out_port_b((uint16_t)PIC1::COMMAND, PIC_END_OF_INTERRUPT);
}

} // namespace cock::arch::x86
