/** @file
 * @brief Thread: an independent execution context with a kernel stack, a
 * message inbox and the state the scheduler and the blocking calls need.
 */
#ifndef CORE_THREAD_HPP
#define CORE_THREAD_HPP

#include <cock/abi/call.hpp>
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

/** @brief Smallest kernel stack of a thread in bytes. */
constexpr size_t MIN_KERNEL_STACK_BYTES = 0x4000;
/** @brief Smallest user stack of a main thread in bytes. */
constexpr size_t MIN_USER_STACK_BYTES = 0x10000;
/** @brief Kernel stack size in pages, rounded up from the minimum. */
constexpr size_t KERNEL_STACK_PAGES =
	(MIN_KERNEL_STACK_BYTES + hal::PAGE_MASK) >> hal::PAGE_SHIFT;
/** @brief Kernel stack size in bytes after rounding to pages. */
constexpr size_t THREAD_STACK_SIZE = KERNEL_STACK_PAGES << hal::PAGE_SHIFT;
/** @brief User stack size in pages, rounded up from the minimum. */
constexpr size_t USER_PAGES =
	(MIN_USER_STACK_BYTES + hal::PAGE_MASK) >> hal::PAGE_SHIFT;
/** @brief User stack size in bytes after rounding to pages. */
constexpr size_t USER_STACK_SIZE = USER_PAGES << hal::PAGE_SHIFT;
/** @brief Number of messages a thread inbox can hold. */
constexpr size_t THREAD_INBOX_CAPACITY = 0x10;
/** @brief Value of the futex address of a thread that is not waiting. */
constexpr uintptr_t NO_FUTEX_ADDRESS = 0;

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
	IDLE = 0, /**< Lowest level, used by the idle thread. */
	LOW = 1, /**< Threads that used up their quantum several times. */
	NORMAL = 2, /**< Default level of new threads. */
	HIGH = 3, /**< Above normal. */
	CRITICAL = 4 /**< Highest level, also the target of the periodic boost. */
};

/**
 * @brief Represents the exit status or error code of a thread.
 */
enum class ThreadError {
	NONE = 0, /**< Finished without error. */
	GENERAL_FAULT = 1, /**< Unspecified failure. */
	TIMEOUT /**< A time limit ran out. */
};

/**
 * @brief Represents the current execution state of a thread.
 */
enum class ThreadState {
	READY, /**< Runnable, normally on a ready queue. */
	RUNNING, /**< Executing on the CPU. */
	BLOCKED, /**< Sleeping on a timer or a futex. */
	WAITING_MSG, /**< Sleeping until an asynchronous message arrives. */
	WAITING_CALL, /**< A server sleeping in accept. */
	WAITING_REPLY, /**< A caller sleeping until its call is answered. */
	DEAD /**< Finished, waiting to be reaped. */
};

/** @brief State of the deadline attached to a thread. */
enum class TimeoutState : uint8_t {
	NONE, /**< No deadline. */
	ARMED, /**< Deadline set and not reached. */
	EXPIRED /**< Deadline reached, not yet consumed by the waiting call. */
};

/**
 * @brief Differentiates between Kernel (Ring 0) and User (Ring 3) threads.
 */
enum class ThreadType { KERNEL, USER };

/**
 * @brief Wrapper for the execution result of a thread function.
 */
class ThreadResult {
  private:
	bool errorFlag; /**< True when an error code other than none is set. */
	ThreadError errorCode; /**< The error code. */

  public:
	/** @brief Creates a successful result. */
	constexpr ThreadResult() : errorFlag(false), errorCode(ThreadError::NONE) {}
	/**
	 * @brief Creates a result from an error code.
	 * @param error Error code, none means success.
	 */
	constexpr ThreadResult(ThreadError error)
		: errorFlag(error == ThreadError::NONE ? false : true),
		  errorCode(error) {}

	/** @return True when the thread failed. */
	bool hasError() const { return errorFlag; }
	/** @return The error code. */
	ThreadError getError() const { return errorCode; }

	/** @return A successful result. */
	static ThreadResult success() { return ThreadResult(); }
	/**
	 * @brief Builds a failed result.
	 * @param error Error code.
	 * @return A result carrying the code.
	 */
	static ThreadResult error(ThreadError error) { return ThreadResult(error); }
};

/** @brief Entry point of a kernel thread. */
using ThreadFunction = ThreadResult (*)();

/**
 * @brief Core Thread class representing an independent execution context.
 *
 * Threads are allocated from a slab pool and registered in the task manager
 * under their id on creation. A thread of a user process has a kernel stack
 * used for system calls and interrupts and a user stack. Fields that the
 * scheduler and the blocking primitives manipulate directly are public.
 */
class Thread {
  private:
	uint32_t id = 0; /**< Thread id, with its generation. */
	ThreadState state = ThreadState::READY; /**< Current state. */
	ThreadPriority priority = ThreadPriority::NORMAL; /**< Scheduling level. */
	ThreadType type = ThreadType::KERNEL; /**< Kernel or user. */
	memory::VirtAddr userStackBase; /**< Lowest address of the user stack. */
	uintptr_t stackPointer = 0; /**< Saved context while not running. */
	Process *process = nullptr; /**< Owning process. */
	void *stackBase = nullptr; /**< Lowest address of the kernel stack. */
	RingBuffer<Message, THREAD_INBOX_CAPACITY + 1> inbox; /**< Pending messages. */
	uint32_t pendingNotifications = 0; /**< Bit mask of interrupt lines raised. */
	uint64_t wakeTick = 0; /**< Tick at which the armed deadline expires. */
	TimeoutState timeoutState = TimeoutState::NONE; /**< Deadline state. */
	uintptr_t futexAddress = NO_FUTEX_ADDRESS; /**< Word being waited on. */
	bool futexWoken = false; /**< A wake arrived and was not consumed yet. */
	/**
	 * @brief Common construction: allocates the id, attaches to the process
	 * and obtains a kernel stack.
	 * @param priority Initial priority.
	 * @param type Kernel or user.
	 * @param owner Owning process.
	 * @param kernel_stack Preallocated kernel stack, or null to allocate one.
	 * @note Panics when the kernel stack cannot be allocated.
	 */
	void initBase(ThreadPriority priority, ThreadType type, Process *owner,
				  void *kernel_stack = nullptr);
	/**
	 * @brief Builds a secondary user thread on a stack the caller supplied.
	 * @param owner Owning process.
	 * @param entry_point User address where it starts.
	 * @param user_stack_base Lowest address of its user stack.
	 * @param user_stack_size Size of its user stack.
	 * @param kernel_stack Kernel stack already allocated.
	 * @param priority Initial priority.
	 */
	Thread(Process &owner, memory::VirtAddr entry_point,
		   memory::VirtAddr user_stack_base, size_t user_stack_size,
		   void *kernel_stack, ThreadPriority priority);

  public:
	Thread *link = nullptr; /**< Ready queue link. */
	Thread *timerLink = nullptr; /**< Link in the list of armed deadlines. */
	Thread *futexLink = nullptr; /**< Link in the futex wait queue. */
	Thread *callLink = nullptr; /**< Link in a server list of callers, deferred
								   calls. */
	Thread *callers = nullptr; /**< Callers waiting for this server. */
	Thread *serving = nullptr; /**< Caller whose request is being served. */
	Thread *deferred = nullptr; /**< Callers detached to be answered later. */
	Thread *callTarget = nullptr; /**< Server this thread has called. */
	uint32_t callWords[abi::call::WORD_COUNT] = {}; /**< Words of the pending
													   request of this caller. */
	bool queued = false; /**< True while on a ready queue. */

	/**
	 * @brief Creates an additional thread in a user process.
	 * @param owner User process.
	 * @param entry_point User address where it starts.
	 * @param user_stack_base Lowest address of a stack supplied by the caller.
	 * @param user_stack_size Size of that stack in bytes.
	 * @param priority Initial priority.
	 * @return The thread, not yet scheduled, or nullptr when the process is the
	 * kernel one, has too many threads, no pid is free or no kernel stack is
	 * available.
	 */
	static Thread *createUser(Process &owner, memory::VirtAddr entry_point,
							  memory::VirtAddr user_stack_base,
							  size_t user_stack_size, ThreadPriority priority);

	/** @return The word the thread waits on, or the no address value. */
	uintptr_t getFutexAddress() const { return futexAddress; }
	/**
	 * @brief Records the futex word being waited on.
	 * @param address User address, or the no address value to clear.
	 */
	void setFutexAddress(uintptr_t address) { futexAddress = address; }
	/** @return True while the thread is queued on a futex. */
	bool isFutexWaiting() const { return futexAddress != NO_FUTEX_ADDRESS; }
	/**
	 * @brief Sets or clears the woken flag.
	 * @param woken New value.
	 */
	void setFutexWoken(bool woken) { futexWoken = woken; }
	/**
	 * @brief Reads and clears the woken flag.
	 * @return True when a wake had been delivered.
	 */
	bool takeFutexWake() {
		bool woken = futexWoken;
		futexWoken = false;
		return woken;
	}

	/** @return The state of the deadline. */
	TimeoutState getTimeoutState() const { return timeoutState; }
	/**
	 * @brief Sets the state of the deadline.
	 * @param state New state.
	 */
	void setTimeoutState(TimeoutState state) { timeoutState = state; }
	/** @return Tick at which the deadline expires. */
	uint64_t getWakeTick() const { return wakeTick; }
	/**
	 * @brief Sets the tick at which the deadline expires.
	 * @param tick Absolute tick.
	 */
	void setWakeTick(uint64_t tick) { wakeTick = tick; }

	/**
	 * @brief Allocates a thread from the pool.
	 * @param size Must be sizeof(Thread).
	 * @return A slot. Panics on a wrong size or when the pool is exhausted.
	 */
	static void *operator new(size_t size);
	/**
	 * @brief Returns a thread slot to the pool.
	 * @param pointer Slot from operator new, null is ignored.
	 */
	static void operator delete(void *pointer);

	/**
	 * @brief Creates a kernel thread in the kernel process.
	 * @param entry_point Function the thread runs, then exits.
	 * @param priority Initial priority.
	 */
	Thread(ThreadFunction entry_point, ThreadPriority priority);
	/**
	 * @brief Creates the first thread of a user process.
	 *
	 * Maps the user stack just below the user space limit of the process.
	 *
	 * @param owner User process without threads.
	 * @param entry_point User address where it starts.
	 * @param priority Initial priority.
	 * @note Panics for the kernel process, a process that already has a thread,
	 * an invalid limit or when the stacks cannot be allocated.
	 */
	Thread(Process &owner, memory::VirtAddr entry_point,
		   ThreadPriority priority);
	/**
	 * @brief Releases everything the thread holds: futex and timer entries,
	 * interrupt lines and kernel stack, and the process if it was the last
	 * thread.
	 */
	~Thread();

	/** @return The thread id. */
	uint32_t getId() const { return this->id; }

	/** @return The current state. */
	ThreadState getState() const { return state; }
	/**
	 * @brief Sets the state. No queue is touched.
	 * @param state New state.
	 */
	void setState(ThreadState state) { this->state = state; }

	/** @return The scheduling priority. */
	ThreadPriority getPriority() const { return priority; }
	/**
	 * @brief Sets the scheduling priority.
	 * @param priority New level, used at the next enqueue.
	 */
	void setPriority(ThreadPriority priority) { this->priority = priority; }

	/** @return Kernel or user. */
	ThreadType getType() const { return type; }

	/** @return The saved context, valid while the thread is not running. */
	uintptr_t getStackPointer() const { return stackPointer; }
	/**
	 * @brief Stores the saved context.
	 * @param stackPointer Context pointer given by the HAL.
	 */
	void setStackPointer(uintptr_t stackPointer) {
		this->stackPointer = stackPointer;
	}

	/** @return Address just above the kernel stack, where the CPU enters from
	 * Ring 3. */
	uintptr_t getKernelStackTop() const {
		return reinterpret_cast<uintptr_t>(stackBase) + THREAD_STACK_SIZE;
	}

	/** @return Lowest address of the user stack. */
	memory::VirtAddr getUserStackBase() const { return userStackBase; }
	/**
	 * @brief Records the user stack.
	 * @param base Lowest address of the user stack.
	 */
	void setUserStackBase(memory::VirtAddr base) { userStackBase = base; }

	/**
	 * @brief Changes where a dormant thread will start executing.
	 * @param entry New entry address.
	 * @note The thread must not have run yet or be switched out.
	 */
	void setEntryPoint(void *entry) {
		hal::update_thread_entry(stackPointer, entry);
	}

	/**
	 * @brief Pushes an incoming IPC message into the thread's inbox.
	 * @param msg Message to copy in.
	 * @return False when the inbox is full and the message was dropped.
	 */
	bool receiveMessage(const Message &msg) { return inbox.push(msg); }
	/**
	 * @brief Pops the oldest message of the inbox.
	 * @param out Receives the message.
	 * @return False when the inbox is empty.
	 */
	bool getNextMessage(Message &out) { return inbox.pop(out); }
	/** @return True when the inbox holds at least one message. */
	bool hasMessages() const { return !inbox.isEmpty(); }

	/**
	 * @brief Marks an interrupt line as raised for this thread.
	 * @param line Line number, below the line count. Repeated raises of the
	 * same line merge into one.
	 */
	void raiseNotification(uint32_t line) {
		pendingNotifications |= uint32_t{1} << line;
	}
	/**
	 * @brief Takes the lowest raised line.
	 * @param line Receives the line number.
	 * @return False when none is raised.
	 */
	bool takeNotification(uint32_t &line) {
		if (pendingNotifications == 0) return false;
		line = static_cast<uint32_t>(__builtin_ctz(pendingNotifications));
		pendingNotifications &= ~(uint32_t{1} << line);
		return true;
	}
	/** @return True when any interrupt line is raised. */
	bool hasNotifications() const { return pendingNotifications != 0; }

	/** @return The owning process. */
	Process *getProcess() const { return process; }
	/** @return Physical root of the address space of the owner. */
	memory::PhysAddr getAddressSpace() const {
		return process->getAddressSpace();
	}
	/** @return User space bound of the owner. */
	memory::VirtAddr getUserSpaceLimit() const {
		return process->getUserSpaceLimit();
	}

	/**
	 * @brief First function a kernel thread runs: calls the thread function
	 * and terminates the thread with its result.
	 * @param userFunc The function to run.
	 * @note Never returns.
	 */
	static void wrapper(ThreadFunction userFunc);
	/** @brief Yields the CPU of the current thread. See Scheduler::yield. */
	static void yield();
};

/** @brief FIFO of threads linked through Thread::link. */
using ThreadQueue = IntrusiveQueue<Thread, &Thread::link>;

} // namespace cock::core::task

#endif // !CORE_THREAD_HPP
