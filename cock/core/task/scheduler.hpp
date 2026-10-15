#ifndef CORE_SCHEDULER_HPP
#define CORE_SCHEDULER_HPP

#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

class Scheduler {
  public:
	static constexpr size_t PRIORITY_NUMBER = 5;

	static void init();
	static void setIdleThread(Thread *thread);
	static void addThread(Thread *thread);
	static uintptr_t onTick(uintptr_t current_stack_pointer);
	static uintptr_t onYield(uintptr_t current_stack_pointer);
	static void terminateCurrentThread();
	static void exitCurrentThread(ThreadResult result);
	static void yield();
	static Thread *getCurrentThread();

  private:
	static constexpr uint32_t QUANTUM = 10;
	static constexpr uint32_t BOOST_TICKS = 1000;
	static ThreadQueue readyQueues[PRIORITY_NUMBER];
	static Thread *currentThread;
	static Thread *idleThread;
	static uint32_t currentTicks;
	static uint32_t ticksSinceLastBoost;
	static bool initialized;
	static void boostPriorities();
	static void enqueue(Thread *thread);
	static Thread *dequeueNext();
	static bool hasReady();
	static uintptr_t switchFrom(uintptr_t current_stack_pointer,
								bool quantum_expired);
};

} // namespace cock::core::task

#endif // !CORE_SCHEDULER_HPP
