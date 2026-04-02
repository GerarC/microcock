#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/heap.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <string.h>

namespace cock::core::task {

using cock::core::memory::kfree;
using cock::core::memory::kmalloc;
using cock::core::memory::VirtualMemoryManager;
using cock::core::memory::VMMPermission;
using cock::utils::Logger;

Thread::Thread(ThreadFunction entry_point, ThreadPriority priority) {
	initBase(priority, ThreadType::KERNEL);
	this->addressSpace = VirtualMemoryManager::getKernelDirectory();
	this->stackPointer =
		hal::prepare_thread_stack(stackBase, THREAD_STACK_SIZE,
								  reinterpret_cast<void *>(&Thread::wrapper),
								  reinterpret_cast<void *>(entry_point));
	TaskManager::registerThread(this);
	Logger::trace("Thread %d created", id);
}

Thread::Thread(const void *code, size_t size, ThreadPriority priority) {
	initBase(priority, ThreadType::USER);
	this->addressSpace = VirtualMemoryManager::createAddressSpace();

	uintptr_t old_cr3 = hal::get_current_address_space();
	hal::switch_address_space(this->addressSpace);

	this->userStackBase =
		VirtualMemoryManager::allocPages(USER_PAGES, VMMPermission::USER_DATA);
	memcpy(this->userStackBase, code, size);

	hal::switch_address_space(old_cr3);

	this->stackPointer = hal::prepare_user_thread_stack(
		stackBase, THREAD_STACK_SIZE, this->userStackBase, USER_STACK_SIZE,
		this->userStackBase, nullptr);
	TaskManager::registerThread(this);
}

void Thread::initBase(ThreadPriority priority, ThreadType type) {
	this->id = TaskManager::allocatePID();
	this->priority = priority;
	this->type = type;
	this->state = ThreadState::READY;
	this->stackBase = kmalloc(THREAD_STACK_SIZE);
	this->archContext = hal::create_thread_context();
}

Thread::~Thread() {
	if (stackBase != nullptr) {
		kfree(stackBase);
		this->stackBase = nullptr;
	}
	if (userStackBase != nullptr) {
		VirtualMemoryManager::freePages(this->userStackBase, USER_PAGES);
		this->userStackBase = nullptr;
	}
	if (this->archContext != nullptr) {
		hal::destroy_thread_context(this->archContext);
		this->archContext = nullptr;
	}
}

void Thread::yield() { Scheduler::yield(); }

void Thread::wrapper(ThreadFunction userFunc) {
	ThreadResult result = userFunc();
	Scheduler::exitCurrentThread(result);
	utils::panic("UNREACHEABLE");
}

} // namespace cock::core::task
