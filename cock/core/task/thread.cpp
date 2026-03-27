#include <cock/core/hal/tasking.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/utils/panic.hpp>
#include <cock/core/memory/heap.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>

namespace cock::core::task {

using cock::core::memory::kfree;
using cock::core::memory::kmalloc;
using cock::utils::Logger;

constexpr size_t THREAD_STACK_SIZE = 0x1000;

Thread::Thread(uint32_t id, ThreadFunction entry_point, ThreadPriority priority)
	: id(id), state(ThreadState::READY), priority(priority), stackPointer(0) {
	this->stackBase = kmalloc(THREAD_STACK_SIZE);
	this->stackPointer =
		hal::prepare_thread_stack(this->stackBase, THREAD_STACK_SIZE,
								  reinterpret_cast<void *>(&Thread::wrapper),
								  reinterpret_cast<void *>(entry_point));

	Logger::trace("Thread %d created", id);
}

Thread::~Thread() {
	if (stackBase != nullptr) {
		kfree(stackBase);
		this->stackBase = nullptr;
	}
}

void Thread::yield() { Scheduler::yield(); }

void Thread::wrapper(ThreadFunction userFunc) {
	ThreadResult result = userFunc();
	Scheduler::exitCurrentThread(result);
	utils::panic("UNREACHEABLE");
}

} // namespace cock::core::task
