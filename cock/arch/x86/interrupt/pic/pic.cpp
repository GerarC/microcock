#include <cock/arch/x86/interrupt/pic/pic.hpp>
#include <hal/cpu.hpp>

namespace cock::arch::x86 {

using core::hal::port_in8;
using core::hal::io_delay;
using core::hal::port_out8;

static uint16_t data_port(uint8_t irq) {
	if (irq < PIC_IRQS_PER_CHIP) return static_cast<uint16_t>(PIC1::DATA);
	return static_cast<uint16_t>(PIC2::DATA);
}

void PIC_remap(uint8_t master_offset, uint8_t slave_offset) {
	port_out8((uint16_t)PIC1::COMMAND, static_cast<uint8_t>(ICW1::INIT) |
											static_cast<uint8_t>(ICW1::ICW4));
	io_delay();
	port_out8((uint16_t)PIC2::COMMAND, static_cast<uint8_t>(ICW1::INIT) |
											static_cast<uint8_t>(ICW1::ICW4));
	io_delay();

	port_out8((uint16_t)PIC1::DATA, master_offset);
	io_delay();
	port_out8((uint16_t)PIC2::DATA, slave_offset);
	io_delay();

	port_out8((uint16_t)PIC1::DATA, 1 << CASCADE_IRQ);
	io_delay();
	port_out8((uint16_t)PIC2::DATA, CASCADE_IRQ);
	io_delay();

	port_out8((uint16_t)PIC1::DATA, static_cast<uint8_t>(ICW4::EES));
	io_delay();
	port_out8((uint16_t)PIC2::DATA, static_cast<uint8_t>(ICW4::EES));
	io_delay();

	port_out8((uint16_t)PIC1::DATA, PIC_MASK_ALL);
	port_out8((uint16_t)PIC2::DATA, PIC_MASK_ALL);
}

void PIC_mask(uint8_t irq) {
	uint16_t port = data_port(irq);
	uint8_t bit = irq % PIC_IRQS_PER_CHIP;
	port_out8(port, port_in8(port) | (1 << bit));
}

void PIC_unmask(uint8_t irq) {
	uint16_t port = data_port(irq);
	uint8_t bit = irq % PIC_IRQS_PER_CHIP;
	port_out8(port, port_in8(port) & ~(1 << bit));

	if (irq >= PIC_IRQS_PER_CHIP) PIC_unmask(CASCADE_IRQ);
}

uint8_t PIC_read_isr(bool slave) {
	uint16_t port = static_cast<uint16_t>(PIC1::COMMAND);
	if (slave) port = static_cast<uint16_t>(PIC2::COMMAND);
	port_out8(port, PIC_READ_ISR);
	io_delay();
	return port_in8(port);
}

void PIC_end_of_interrupt(uint8_t irq) {
	if (irq >= PIC_IRQS_PER_CHIP)
		port_out8((uint16_t)PIC2::COMMAND, PIC_END_OF_INTERRUPT);
	port_out8((uint16_t)PIC1::COMMAND, PIC_END_OF_INTERRUPT);
}

} // namespace cock::arch::x86
