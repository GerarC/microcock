#ifndef CORE_SCHEDULER_HPP
#define CORE_SCHEDULER_HPP

#include <cock/utils/data_structure/ll.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>
#include <stddef.h>

namespace cock::core::task {
using utils::data_structure::LinkedList;

class Scheduler {
  public:
    static constexpr size_t PRIORITY_NUMBER = 5;

	static void init();
	static void addThread(Thread *thread);
	static uintptr_t schedule(uintptr_t current_stack_pointer);
	static void exitCurrentThread(ThreadResult result);
	static void yield();
	static Thread *getCurrentThread();

  private:
    static constexpr uint32_t QUANTUM = 10;
    static constexpr uint32_t BOOST_TICKS = 1000;
	static  LinkedList<Thread*> readyQueues[PRIORITY_NUMBER];
	static Thread *currentThread;
    static uint32_t currentTicks;
    static uint32_t ticksSinceLastBoost;
    static bool initialized;
    static void boostPriorities();
};

} // namespace cock::core::task

#endif // !CORE_SCHEDULER_HPP
