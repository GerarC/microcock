#ifndef CORE_THREAD_HPP
#define CORE_THREAD_HPP

#include "cock/data_structure/ll.hpp"
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/memory_region.hpp>
#include <cock/data_structure/ring_buffer.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

constexpr size_t THREAD_STACK_SIZE = 0x1000;
constexpr size_t USER_STACK_SIZE = 0x10000;
constexpr size_t USER_PAGES = USER_STACK_SIZE / 0x1000;
constexpr size_t THREAD_MESSAGE_BUFFER_SIZE = 0x10;

using cock::core::hal::ArchThreadContext;
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
	uint32_t id;			 /**< Unique Process Identifier (PID). */
	ThreadState state;		 /**< Current execution state. */
	ThreadPriority priority; /**< Scheduler priority level. */
	ThreadType type;		 /**< Ring 0 or Ring 3 execution type. */
	void *userStackBase;	 /**< Base address of the Userland stack. */
	uintptr_t stackPointer;	 /**< Saved stack pointer for context switching. */
	uintptr_t addressSpace;	 /**< CR3 / Page Directory physical address. */
	void *stackBase;		 /**< Base address of the Kernel stack. */
	RingBuffer<Message, THREAD_MESSAGE_BUFFER_SIZE>
		inbox;					   /**< IPC message queue. */
	ArchThreadContext archContext; /**< Hardware-specific execution context
									  (IOPM, registers). */
	LinkedList<MemoryRegion>
		mapped_pages; /**< Dynamically allocated virtual memory regions. */

	void initBase(ThreadPriority priority, ThreadType type);

  public:
	/**
	 * @brief Constructs a new Kernel Thread.
	 */
	Thread(ThreadFunction entry_point, ThreadPriority priority);

	/**
	 * @brief Constructs a new User Thread from a binary payload in memory.
	 */
	Thread(const void *code, size_t size, ThreadPriority priority);

	/**
	 * @brief Constructs a new User Thread with a pre-existing address space
	 * (e.g., loaded ELF).
	 */
	Thread(uintptr_t entry_point, uintptr_t address_space,
		   ThreadPriority priority);
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

	ArchThreadContext getArchContext() const { return archContext; }
	void setArchContext(ArchThreadContext *context) {
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
	void receiveMessage(const Message &msg) { inbox.push(msg); }
	bool getNextMessage(Message &out) { return inbox.pop(out); }
	bool hasMessages() const { return !inbox.isEmpty(); }

	uintptr_t getAddressSpace() const { return addressSpace; }

	/**
	 * @brief Registers dynamically allocated memory to track and prevent memory
	 * leaks upon thread death.
	 */
	void addMemoryRegion(uintptr_t addr, size_t pages) {
		mapped_pages.append({addr, pages});
	}
	void removeMemoryRegion(uintptr_t addr) { mapped_pages.remove({addr, 0}); }

	static void wrapper(ThreadFunction userFunc);
	static void yield();
};

} // namespace cock::core::task

#endif // !CORE_THREAD_HPP
