#ifndef CORE_FUTEX_HPP
#define CORE_FUTEX_HPP

#include <cock/core/task/process.hpp>
#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

class Futex {
  public:
	static void enqueue(Thread *thread, uintptr_t address);
	static void remove(Thread *thread);
	static size_t wake(const Process *process, uintptr_t address, size_t count);
	static size_t waiterCount();

  private:
	static Thread *waiters;
};

} // namespace cock::core::task

#endif // !CORE_FUTEX_HPP
