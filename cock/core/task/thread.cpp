#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <string.h>

namespace cock::core::task {

using cock::core::memory::VirtualMemoryManager;
using cock::core::memory::PhysAddr;
using cock::core::memory::VirtAddr;
using cock::core::memory::VMMPermission;
using cock::utils::Logger;
using hal::InterruptGuard;

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

Thread::Thread(VirtAddr entry_point, PhysAddr address_space,
			   VirtAddr user_space_limit, ThreadPriority priority) {
	if (address_space.isNull()) utils::panic("Thread: invalid address space");
	if (user_space_limit.raw() <= USER_STACK_SIZE ||
		user_space_limit > memory::USER_SPACE_END)
		utils::panic("Thread: invalid user space limit");

	initBase(priority, ThreadType::USER);
	this->addressSpace = address_space;
	this->userSpaceLimit = user_space_limit;
	this->mmapCursor = VirtAddr(hal::USER_MMAP_BASE);

	{
		InterruptGuard guard;
		PhysAddr old_cr3 = hal::get_current_address_space();
		hal::switch_address_space(this->addressSpace);

		this->userStackBase = VirtualMemoryManager::mapPagesAt(
			VirtAddr(user_space_limit.raw() - USER_STACK_SIZE), USER_PAGES,
			VMMPermission::USER_DATA, user_space_limit);

		hal::switch_address_space(old_cr3);
	}
	if (this->userStackBase.isNull()) utils::panic("Thread: cannot map user stack");

	this->stackPointer = hal::prepare_user_thread_stack(
		stackBase, THREAD_STACK_SIZE, this->userStackBase.as<void>(),
		USER_STACK_SIZE, entry_point.as<void>(), nullptr);

	TaskManager::registerThread(this);
}

void Thread::initBase(ThreadPriority priority, ThreadType type) {
	this->id = TaskManager::allocatePID();
	this->priority = priority;
	this->type = type;
	this->state = ThreadState::READY;
	this->stackBase = VirtualMemoryManager::allocStack(KERNEL_STACK_PAGES);
	if (!this->stackBase) utils::panic("Thread: cannot allocate kernel stack");
	this->archContext = hal::create_thread_context();
}

Thread::~Thread() {
	if (type == ThreadType::USER && !addressSpace.isNull() &&
		addressSpace != VirtualMemoryManager::getKernelDirectory())
		VirtualMemoryManager::destroyAddressSpace(addressSpace);
	this->userStackBase = VirtAddr();

	if (stackBase != nullptr) {
		VirtualMemoryManager::freeStack(stackBase, KERNEL_STACK_PAGES);
		this->stackBase = nullptr;
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
