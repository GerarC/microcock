#include <cock/arch/x86/interrupt/irq/irq.hpp>
#include <cock/arch/x86/timer/timer.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/utils/logger.hpp>

namespace cock::arch::x86 {

using core::hal::port_out8;
using  cock::utils::Logger;

constexpr uint32_t INITIAL_TICKS = 0;
constexpr uint32_t IRQ0_INDEX = 0x20;
constexpr uint32_t BASE_FREQUENCY = 0x1234DC; // 1193180
constexpr uint8_t PIT_CHANNEL0_SQUARE_WAVE = 0x36;
constexpr uint32_t DIVISOR_BYTE_MASK = 0xFF;
constexpr uint32_t DIVISOR_HIGH_SHIFT = 8;

volatile uint64_t Timer::ticks = INITIAL_TICKS;

void Timer::onIrq0(InterruptRegisters *regs) {
	(void)regs;
	ticks = ticks + 1;
}

uint64_t Timer::getTicks() {
	core::hal::InterruptGuard guard;
	return ticks;
}

void Timer::init() {
	ticks = INITIAL_TICKS;

	// Mhz 11931816666
	uint32_t divisor = BASE_FREQUENCY / FREQUENCY;

	port_out8(static_cast<uint16_t>(Output::COMMAND_REGISTER),
			  PIT_CHANNEL0_SQUARE_WAVE);
	port_out8(static_cast<uint16_t>(Output::CHANNEL0),
			   static_cast<uint8_t>(divisor & DIVISOR_BYTE_MASK));
	port_out8(static_cast<uint16_t>(Output::CHANNEL0),
			   static_cast<uint8_t>((divisor >> DIVISOR_HIGH_SHIFT) & DIVISOR_BYTE_MASK));

	irq_install_handler(IRQ0_INDEX, &Timer::onIrq0);

    Logger::debug("Timer Initialized");
}

} // namespace cock::arch::x86
