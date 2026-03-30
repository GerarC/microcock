#ifndef CORE_THREAD_HPP
#define CORE_THREAD_HPP

#include <cock/core/hal/tasking.hpp>
#include <cock/data_structure/ring_buffer.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

constexpr size_t THREAD_STACK_SIZE = 0x1000;
constexpr size_t USER_STACK_SIZE = 0x10000;
constexpr size_t USER_PAGES = USER_STACK_SIZE / 0x1000;
constexpr size_t THREAD_MESSAGE_BUFFER_SIZE = 0x10;

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
	uint32_t id;
	ThreadState state;
	ThreadPriority priority;
	ThreadType type;
	void *userStackBase;
	uintptr_t stackPointer;
	uintptr_t addressSpace;
	// pointer to the real base given by the heap
	void *stackBase;
	RingBuffer<Message, THREAD_MESSAGE_BUFFER_SIZE> inbox;
	void initBase(ThreadPriority priority, ThreadType type);

  public:
	Thread(ThreadFunction entry_point, ThreadPriority priority);
	Thread(const void *code, size_t size, ThreadPriority priority,
		   bool is_driver = false);
	~Thread();

	uint32_t getId() const { return this->id; }

	ThreadState getState() const { return state; }
	void setState(ThreadState state) { this->state = state; }

	ThreadPriority getPriority() const { return priority; }
	void setPriority(ThreadPriority priority) { this->priority = priority; }

	uintptr_t getStackPointer() const { return stackPointer; }
	void setStackPointer(uintptr_t stackPointer) {
		this->stackPointer = stackPointer;
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
	void setUserStackBase(void *base) { userStackBase = base; }

	static void wrapper(ThreadFunction userFunc);
	static void yield();
};

} // namespace cock::core::task

#endif // !CORE_THREAD_HPP
