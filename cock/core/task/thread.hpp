#ifndef CORE_THREAD_HPP
#define CORE_THREAD_HPP

#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/data_structure/intrusive_queue.hpp>
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
using cock::data_structure::RingBuffer;
using ipc::Message;

enum class ThreadPriority : uint8_t {
	IDLE = 0,
	LOW = 1,
	NORMAL = 2,
	HIGH = 3,
	CRITICAL = 4
};

enum class ThreadError { NONE = 0, GENERAL_FAULT = 1, TIMEOUT };

enum class ThreadState { READY, RUNNING, BLOCKED, WAITING_MSG, DEAD };

enum class ThreadType { KERNEL, USER };

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

	void setEntryPoint(void *entry) {
		hal::update_thread_entry(stackPointer, entry);
	}

	void receiveMessage(const Message &msg) { inbox.push(msg); }
	bool getNextMessage(Message &out) { return inbox.pop(out); }
	bool hasMessages() const { return !inbox.isEmpty(); }

	uintptr_t getAddressSpace() const { return addressSpace; }
	uintptr_t getUserSpaceLimit() const { return userSpaceLimit; }
	void setUserStackBase(void *base) { userStackBase = base; }

	static void wrapper(ThreadFunction userFunc);
	static void yield();
};

using ThreadQueue = IntrusiveQueue<Thread, &Thread::link>;

} // namespace cock::core::task

#endif // !CORE_THREAD_HPP
