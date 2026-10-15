#ifndef CORE_THREAD_HPP
#define CORE_THREAD_HPP

#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <cock/data_structure/intrusive_queue.hpp>
#include <cock/data_structure/ring_buffer.hpp>
#include <cock/ipc/message.hpp>
#include <hal/interrupt.hpp>
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
constexpr size_t THREAD_INBOX_CAPACITY = 0x10;

static_assert(hal::IRQ_LINE_COUNT <= sizeof(uint32_t) * 8,
			  "every interrupt line needs a bit in the pending mask");

using cock::data_structure::IntrusiveQueue;
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

enum class TimeoutState : uint8_t { NONE, ARMED, EXPIRED };

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
	memory::VirtAddr userStackBase;
	uintptr_t stackPointer = 0;
	Process *process = nullptr;
	void *stackBase = nullptr;
	RingBuffer<Message, THREAD_INBOX_CAPACITY + 1> inbox;
	uint32_t pendingNotifications = 0;
	uint64_t wakeTick = 0;
	TimeoutState timeoutState = TimeoutState::NONE;
	void initBase(ThreadPriority priority, ThreadType type, Process *owner);

  public:
	Thread *link = nullptr;
	Thread *timerLink = nullptr;
	bool queued = false;

	TimeoutState getTimeoutState() const { return timeoutState; }
	void setTimeoutState(TimeoutState state) { timeoutState = state; }
	uint64_t getWakeTick() const { return wakeTick; }
	void setWakeTick(uint64_t tick) { wakeTick = tick; }

	static void *operator new(size_t size);
	static void operator delete(void *pointer);

	Thread(ThreadFunction entry_point, ThreadPriority priority);
	Thread(Process &owner, memory::VirtAddr entry_point,
		   ThreadPriority priority);
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

	uintptr_t getKernelStackTop() const {
		return reinterpret_cast<uintptr_t>(stackBase) + THREAD_STACK_SIZE;
	}

	memory::VirtAddr getUserStackBase() const { return userStackBase; }
	void setUserStackBase(memory::VirtAddr base) { userStackBase = base; }

	void setEntryPoint(void *entry) {
		hal::update_thread_entry(stackPointer, entry);
	}

	/**
	 * @brief Pushes an incoming IPC message into the thread's inbox.
	 */
	bool receiveMessage(const Message &msg) { return inbox.push(msg); }
	bool getNextMessage(Message &out) { return inbox.pop(out); }
	bool hasMessages() const { return !inbox.isEmpty(); }

	void raiseNotification(uint32_t line) {
		pendingNotifications |= uint32_t{1} << line;
	}
	bool takeNotification(uint32_t &line) {
		if (pendingNotifications == 0) return false;
		line = static_cast<uint32_t>(__builtin_ctz(pendingNotifications));
		pendingNotifications &= ~(uint32_t{1} << line);
		return true;
	}
	bool hasNotifications() const { return pendingNotifications != 0; }

	Process *getProcess() const { return process; }
	memory::PhysAddr getAddressSpace() const {
		return process->getAddressSpace();
	}
	memory::VirtAddr getUserSpaceLimit() const {
		return process->getUserSpaceLimit();
	}

	static void wrapper(ThreadFunction userFunc);
	static void yield();
};

using ThreadQueue = IntrusiveQueue<Thread, &Thread::link>;

} // namespace cock::core::task

#endif // !CORE_THREAD_HPP
