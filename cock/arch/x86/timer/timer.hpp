#ifndef ARCH_X86_TIMER_HPP
#define ARCH_X86_TIMER_HPP

#include <cock/arch/x86/utils/helpers.hpp>
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
	static constexpr uint32_t FREQUENCY = 100;

	/**
	 * @brief Retrieves the total number of ticks elapsed since the timer was
	 * initialized.
	 */
	static uint64_t getTicks();

	/**
	 * @brief Configures the PIT frequency and installs the IRQ0 handler.
	 */
	static void init();

  private:
	static volatile uint64_t ticks;

	/**
	 * @brief Internal handler executed every time IRQ0 fires.
	 */
	static void onIrq0(InterruptRegisters *regs);
};

} // namespace cock::arch::x86

#endif // !TIMER_HPP
