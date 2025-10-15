#ifndef TIMER_HPP
#define TIMER_HPP

#include "interrupt/idt/idt.hpp"
#include "interrupt/utils/functs.hpp"
#include <stdint.h>

namespace cock::arch::x86 {
using utils::InterruptRegisters;

enum class Output : uint16_t {
	CHANNEL0 = 0x40,
	CHANNEL1 = 0x41,
	CHANNEL2 = 0x42,
	COMMAND_REGISTER = 0x43,
};

class Timer {
  public:
	static constexpr uint32_t FREQUENCY = 100;
    static uint64_t getTicks();
	void init();


  private:
	static volatile uint64_t ticks;
	static void onIrq0(InterruptRegisters *regs);
};

} // namespace cock::arch::x86

#endif // !TIMER_HPP
