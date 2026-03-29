#include <cock/core/task/task_manager.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/heap.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>

namespace cock::core::task {

using cock::core::memory::kfree;
using cock::core::memory::kmalloc;
using cock::utils::Logger;

constexpr size_t THREAD_STACK_SIZE = 0x1000;

Thread::Thread(ThreadFunction entry_point, ThreadPriority priority)
	: state(ThreadState::READY), priority(priority), stackPointer(0) {
	this->id = TaskManager::allocatePID();
	this->stackBase = kmalloc(THREAD_STACK_SIZE);
	this->stackPointer =
		hal::prepare_thread_stack(this->stackBase, THREAD_STACK_SIZE,
								  reinterpret_cast<void *>(&Thread::wrapper),
								  reinterpret_cast<void *>(entry_point));

    TaskManager::registerThread(this);
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
