/**
 * @file joinable_thread.hpp
 * @brief A thread that another thread of the process can wait for (`join`).
 *
 * Built only on `thread_start`, the futex calls and `process_alive`, so the kernel keeps
 * no extra record of thread exits. The program still owns the stack.
 */
#ifndef COCK_USER_JOINABLE_THREAD_HPP
#define COCK_USER_JOINABLE_THREAD_HPP

#include <cock/user/api.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user {

/**
 * @brief One thread with a join. The object must stay alive and at the same address from
 *        `start` until `join` returns true.
 *
 * `join` returns only when the kernel no longer lists the thread, so the stack can be reused
 * or freed at once. The thread function must return; a function that calls `exit_process`
 * is never reported as finished. Only one thread may call `join` on an object.
 */
class JoinableThread {
  public:
	JoinableThread() = default;
	JoinableThread(const JoinableThread &) = delete;
	JoinableThread &operator=(const JoinableThread &) = delete;

	/**
	 * @brief Starts `function(argument)` on a caller supplied stack.
	 * @param function Thread entry point.
	 * @param argument Pointer passed to `function`.
	 * @param stack Lowest address of a writable stack range owned by the caller.
	 * @param stack_size Size of the range in bytes; at least `MIN_THREAD_STACK_BYTES`.
	 * @return true when the thread runs. False when this object has a thread that was not
	 *         joined yet, or `thread_start` failed (stack too small, bad ranges, thread limit).
	 */
	bool start(ThreadFunction function, void *argument, void *stack, size_t stack_size) {
		uint32_t seen = __atomic_load_n(&state, __ATOMIC_SEQ_CST);
		if (seen != NOT_STARTED && seen != JOINED) return false;

		entry = function;
		entryArgument = argument;
		__atomic_store_n(&state, RUNNING, __ATOMIC_SEQ_CST);
		id = thread_start(&JoinableThread::run, this, stack, stack_size);
		if (id == NO_PROCESS) {
			__atomic_store_n(&state, NOT_STARTED, __ATOMIC_SEQ_CST);
			return false;
		}
		return true;
	}

	/**
	 * @brief Waits until the thread function returned and the kernel dropped the thread.
	 * @param milliseconds Longest wait for the function to return; `FUTEX_NO_TIMEOUT` waits
	 *        without limit. After the function returned, the wait for the kernel to drop
	 *        the thread takes microseconds and has no limit.
	 * @return true when the thread ended and was joined. False when the time ran out (call
	 *         again to keep waiting), when nothing was started, or when it was joined before.
	 */
	bool join(uint32_t milliseconds = abi::process::FUTEX_NO_TIMEOUT) {
		for (;;) {
			uint32_t seen = __atomic_load_n(&state, __ATOMIC_SEQ_CST);
			if (seen == FINISHED) break;
			if (seen != RUNNING) return false;

			uintptr_t result = futex_wait(&state, RUNNING, milliseconds);
			if (result != abi::process::FUTEX_WOKEN &&
				result != abi::process::FUTEX_VALUE_CHANGED)
				return false;
		}

		while (process_alive(id)) sleep_ms(YIELD_ONLY);
		__atomic_store_n(&state, JOINED, __ATOMIC_SEQ_CST);
		return true;
	}

  private:
	/** State: no thread was started, or the last `start` failed. */
	static constexpr uint32_t NOT_STARTED = 0;
	/** State: the thread function has not returned yet. */
	static constexpr uint32_t RUNNING = 1;
	/** State: the thread function returned; the thread may still be leaving. */
	static constexpr uint32_t FINISHED = 2;
	/** State: `join` completed. */
	static constexpr uint32_t JOINED = 3;
	/** Argument of `sleep_ms` that only yields the CPU. */
	static constexpr uint32_t YIELD_ONLY = 0;

	/** Entry given to the kernel: runs the function, publishes the end and wakes the joiner. */
	static void run(void *self) {
		JoinableThread *thread = static_cast<JoinableThread *>(self);
		thread->entry(thread->entryArgument);
		__atomic_store_n(&thread->state, FINISHED, __ATOMIC_SEQ_CST);
		futex_wake(&thread->state, abi::process::FUTEX_WAKE_ALL);
	}

	/** Futex word that holds the state. */
	uint32_t state = NOT_STARTED;
	/** Thread id returned by `thread_start`. */
	uint32_t id = NO_PROCESS;
	/** Function the thread runs. */
	ThreadFunction entry = nullptr;
	/** Argument passed to `entry`. */
	void *entryArgument = nullptr;
};

} // namespace cock::user

#endif // !COCK_USER_JOINABLE_THREAD_HPP
