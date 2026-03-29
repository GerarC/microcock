#ifndef CORE_THREAD_HPP
#define CORE_THREAD_HPP

#include <cock/data_structure/ll.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

using cock::data_structure::LinkedList;
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
	uintptr_t stackPointer;
	// pointer to the real base given by the heap
	void *stackBase;
	LinkedList<Message> inbox;

  public:
	Thread(ThreadFunction entry_point,
		   ThreadPriority priority = ThreadPriority::NORMAL);
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

	void receiveMessage(const Message &msg) { inbox.append(msg); }
	bool getNextMessage(Message &out) { return inbox.popFront(out); }
	bool hasMessages() const { return !inbox.isEmpty(); }

	static void wrapper(ThreadFunction userFunc);
	static void yield();
};

} // namespace cock::core::task

#endif // !CORE_THREAD_HPP
