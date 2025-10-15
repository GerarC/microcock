#include "timer.hpp"
#include "interrupt/irq/irq.hpp"
#include "interrupt/utils/functs.hpp"

namespace cock::arch::x86 {

using utils::out_port_b;

constexpr uint32_t INITIAL_TICKS = 0;
constexpr uint32_t IRQ0_INDEX = 32;
constexpr uint32_t BASE_FREQUENCY = 1193180;

volatile uint64_t Timer::ticks = INITIAL_TICKS;

void Timer::onIrq0(InterruptRegisters *regs) {
	(void)regs;
	ticks++;
}

uint64_t Timer::getTicks(){
    return ticks;
}

void Timer::init() {
	ticks = INITIAL_TICKS;
	irq_install_handler(IRQ0_INDEX, &Timer::onIrq0);

	// Mhz 11931816666
	uint32_t divisor = BASE_FREQUENCY / FREQUENCY;

	out_port_b(static_cast<uint16_t>(Output::COMMAND_REGISTER), 0x36);
	out_port_b(static_cast<uint16_t>(Output::CHANNEL0),
			   static_cast<uint8_t>(divisor & 0xFF));
	out_port_b(static_cast<uint16_t>(Output::CHANNEL0),
			   static_cast<uint8_t>((divisor >> 8) & 0xFF));
}

} // namespace cock::arch::x86
