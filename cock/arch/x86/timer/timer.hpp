#ifndef ARCH_X86_TIMER_HPP
#define ARCH_X86_TIMER_HPP

/**
 * @file
 * @brief System tick source: the 8253/8254 Programmable Interval Timer (PIT)
 * channel 0 wired to IRQ0.
 */

#include <cock/arch/x86/utils/helpers.hpp>
#include <hal/cpu.hpp>
#include <stdint.h>

namespace cock::arch::x86 {
using utils::InterruptRegisters;

/**
 * @brief I/O Ports for the Programmable Interval Timer (PIT).
 */
enum class Output : uint16_t {
	CHANNEL0 = 0x40, /**< Channel 0 data port (connected to IRQ0 for timing). */
	CHANNEL1 = 0x41, /**< Channel 1 data port (obsolete). */
	CHANNEL2 = 0x42, /**< Channel 2 data port (PC speaker). */
	COMMAND_REGISTER = 0x43, /**< PIT Command configuration register. */
};

/**
 * @brief Hardware Timer driver.
 * * Drives the core OS scheduler by triggering IRQ0 at a fixed frequency.
 */
class Timer {
  public:
	/** @brief Target interrupt frequency in Hz (ticks per second). */
	static constexpr uint32_t FREQUENCY = core::hal::TIMER_FREQUENCY_HZ;

	/**
	 * @brief Retrieves the total number of ticks elapsed since the timer was
	 * initialized.
	 *
	 * Reads the 64 bit counter with interrupts disabled so a tick cannot tear
	 * the value. Safe from any Ring 0 context.
	 *
	 * @return Number of IRQ0 ticks since init().
	 */
	static uint64_t getTicks();

	/**
	 * @brief Configures the PIT frequency and installs the IRQ0 handler.
	 *
	 * Resets the tick counter, programs channel 0 in square wave mode with the
	 * divisor `1193180 / FREQUENCY` (command byte to port 0x43, then the low
	 * and high divisor bytes to port 0x40) and registers onIrq0, which unmasks
	 * IRQ0 in the PIC. The PIC must already be remapped. Once, during boot.
	 */
	static void init();

  private:
	/** @brief Tick counter incremented by the IRQ0 handler. */
	static volatile uint64_t ticks;

	/**
	 * @brief Internal handler executed every time IRQ0 fires.
	 *
	 * Increments the tick counter. Runs in Ring 0 inside the interrupt; the
	 * scheduler decision is taken later by the core event hook.
	 *
	 * @param regs Saved register frame of the interrupted context (unused).
	 */
	static void onIrq0(InterruptRegisters *regs);
};

} // namespace cock::arch::x86

#endif // !TIMER_HPP
