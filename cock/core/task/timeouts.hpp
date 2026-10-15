#ifndef CORE_TIMEOUTS_HPP
#define CORE_TIMEOUTS_HPP

#include <cock/core/task/thread.hpp>
#include <hal/cpu.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

enum class TimeoutPhase { TIMED_OUT, WAITING };

class Timeouts {
  public:
	static constexpr uint64_t MILLISECONDS_PER_SECOND = 1000;
	static constexpr uint64_t TICK_MARGIN = 1;

	static uint64_t now() { return ticks; }
	static uint64_t ticksFor(uint32_t milliseconds);
	static size_t armedCount();

	static void arm(Thread *thread, uint64_t delay);
	static void cancel(Thread *thread);
	static void onTick();
	static TimeoutPhase wait(Thread &thread, uint32_t milliseconds,
							 ThreadState blockedState);

  private:
	static uint64_t ticks;
	static Thread *armed;
};

} // namespace cock::core::task

#endif // !CORE_TIMEOUTS_HPP
