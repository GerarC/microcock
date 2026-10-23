/** @file
 * @brief Kernel side of the futex: a queue of threads sleeping on user words.
 */
#ifndef CORE_FUTEX_HPP
#define CORE_FUTEX_HPP

#include <cock/core/task/process.hpp>
#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

/**
 * @brief Global FIFO of threads waiting on a futex word.
 *
 * A waiter is identified by its process and the user address of the word, so
 * the same address in two processes never matches. The queue is intrusive,
 * linked through Thread::futexLink, and needs no allocation. Every operation
 * blocks interrupts while it runs.
 */
class Futex {
  public:
	/**
	 * @brief Appends a thread to the wait queue for an address.
	 * @param thread Thread about to sleep, null is ignored. If it is already
	 * queued it is moved to the tail.
	 * @param address User address of the word, already validated.
	 */
	static void enqueue(Thread *thread, uintptr_t address);
	/**
	 * @brief Takes a thread out of the queue and clears its futex address.
	 * @param thread Thread to remove, null or not waiting is harmless.
	 */
	static void remove(Thread *thread);
	/**
	 * @brief Wakes waiters of a word, oldest first.
	 * @param process Process owning the word.
	 * @param address User address of the word.
	 * @param count Maximum number of threads to wake.
	 * @return Number of threads woken. Each gets its woken flag set, its
	 * timeout cancelled and, if blocked, is put back on the ready queues.
	 */
	static size_t wake(const Process *process, uintptr_t address, size_t count);
	/** @return Number of threads currently queued, mostly for tests. */
	static size_t waiterCount();

  private:
	static Thread *waiters; /**< Head of the wait queue. */
};

} // namespace cock::core::task

#endif // !CORE_FUTEX_HPP
