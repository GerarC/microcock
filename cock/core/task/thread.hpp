#ifndef CORE_THREAD_HPP
#define CORE_THREAD_HPP

#include "cock/data_structure/ll.hpp"
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/data_structure/intrusive_queue.hpp>
#include <cock/core/memory/memory_region.hpp>
#include <cock/data_structure/ring_buffer.hpp>
#include <cock/ipc/message.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

constexpr size_t MIN_KERNEL_STACK_BYTES = 0x4000;
constexpr size_t MIN_USER_STACK_BYTES = 0x10000;
constexpr size_t KERNEL_STACK_PAGES =
	(MIN_KERNEL_STACK_BYTES + hal::PAGE_MASK) >> hal::PAGE_SHIFT;
constexpr size_t THREAD_STACK_SIZE = KERNEL_STACK_PAGES << hal::PAGE_SHIFT;
constexpr size_t USER_PAGES =
	(MIN_USER_STACK_BYTES + hal::PAGE_MASK) >> hal::PAGE_SHIFT;
constexpr size_t USER_STACK_SIZE = USER_PAGES << hal::PAGE_SHIFT;
constexpr size_t THREAD_MESSAGE_BUFFER_SIZE = 0x10;

using cock::core::hal::ArchThreadContext;
using cock::data_structure::IntrusiveQueue;
using cock::core::memory::MemoryRegion;
using cock::data_structure::LinkedList;
using cock::data_structure::RingBuffer;
using ipc::Message;

/**
 * @brief Defines the priority levels for the Multi-Level Feedback Queue
 * scheduler.
 */
enum class ThreadPriority : uint8_t {
	IDLE = 0,
	LOW = 1,
	NORMAL = 2,
	HIGH = 3,
	CRITICAL = 4
};

/**
 * @brief Represents the exit status or error code of a thread.
 */
enum class ThreadError { NONE = 0, GENERAL_FAULT = 1, TIMEOUT };

/**
 * @brief Represents the current execution state of a thread.
 */
enum class ThreadState { READY, RUNNING, BLOCKED, WAITING_MSG, DEAD };

/**
 * @brief Differentiates between Kernel (Ring 0) and User (Ring 3) threads.
 */
enum class ThreadType { KERNEL, USER };

/**
 * @brief Wrapper for the execution result of a thread function.
 */
class ThreadResult {
  private:
	bool errorFlag;
	ThreadError errorCode;

  public:
	constexpr ThreadResult() : errorFlag(false), errorCode(ThreadError::NONE) {}
	constexpr ThreadResult(ThreadError error)
		: errorFlag(error == ThreadError::NONE ? false : true),
		  errorCode(error) {}

	bool hasError() const { return errorFlag; }
	ThreadError getError() const { return errorCode; }

	static ThreadResult success() { return ThreadResult(); }
	static ThreadResult error(ThreadError error) { return ThreadResult(error); }
};

using ThreadFunction = ThreadResult (*)();

/**
 * @brief Core Thread class representing an independent execution context.
 */
class Thread {
  private:
	uint32_t id = 0;
	ThreadState state = ThreadState::READY;
	ThreadPriority priority = ThreadPriority::NORMAL;
	ThreadType type = ThreadType::KERNEL;
	void *userStackBase = nullptr;
	uintptr_t stackPointer = 0;
	uintptr_t addressSpace = 0;
	uintptr_t userSpaceLimit = 0;
	void *stackBase = nullptr;
	RingBuffer<Message, THREAD_MESSAGE_BUFFER_SIZE> inbox;
	LinkedList<MemoryRegion> mapped_pages;
	uintptr_t mmapCursor = 0;
	void initBase(ThreadPriority priority, ThreadType type);
	ArchThreadContext archContext = nullptr;

  public:
	Thread *link = nullptr;
	bool queued = false;

	Thread(ThreadFunction entry_point, ThreadPriority priority);
	Thread(uintptr_t entry_point, uintptr_t address_space,
		   uintptr_t user_space_limit, ThreadPriority priority);
	~Thread();

	uint32_t getId() const { return this->id; }

	ThreadState getState() const { return state; }
	void setState(ThreadState state) { this->state = state; }

	ThreadPriority getPriority() const { return priority; }
	void setPriority(ThreadPriority priority) { this->priority = priority; }

	ThreadType getType() const { return type; }

	uintptr_t getStackPointer() const { return stackPointer; }
	void setStackPointer(uintptr_t stackPointer) {
		this->stackPointer = stackPointer;
	}

	ArchThreadContext getArchContext() const { return archContext; }
	void setArchContext(ArchThreadContext context) {
		this->archContext = context;
	}

	uintptr_t getKernelStackTop() const {
		return reinterpret_cast<uintptr_t>(stackBase) + THREAD_STACK_SIZE;
	}

	void *getUserStackBase() const { return userStackBase; }
	void setUserStackBase(void *base) { userStackBase = base; }

	void setEntryPoint(void *entry) {
		hal::update_thread_entry(stackPointer, entry);
	}

	/**
	 * @brief Pushes an incoming IPC message into the thread's inbox.
	 */
	bool receiveMessage(const Message &msg) { return inbox.push(msg); }
	bool getNextMessage(Message &out) { return inbox.pop(out); }
	bool hasMessages() const { return !inbox.isEmpty(); }

	uintptr_t getAddressSpace() const { return addressSpace; }
	uintptr_t getUserSpaceLimit() const { return userSpaceLimit; }

	/**
	 * @brief Registers dynamically allocated memory to track and prevent memory
	 * leaks upon thread death.
	 */
	void addMemoryRegion(uintptr_t addr, size_t pages) {
		mapped_pages.append({addr, pages});
	}
	void removeMemoryRegion(uintptr_t addr) { mapped_pages.remove({addr, 0}); }

	bool takeMemoryRegion(uintptr_t addr, size_t pages) {
		auto found = mapped_pages.find({addr, 0});
		if (found == mapped_pages.end() || (*found).page_count != pages)
			return false;
		mapped_pages.remove({addr, 0});
		return true;
	}

	uintptr_t getMmapCursor() const { return mmapCursor; }
	void setMmapCursor(uintptr_t cursor) { mmapCursor = cursor; }

	static void wrapper(ThreadFunction userFunc);
	static void yield();
};

using ThreadQueue = IntrusiveQueue<Thread, &Thread::link>;

} // namespace cock::core::task

#endif // !CORE_THREAD_HPP
