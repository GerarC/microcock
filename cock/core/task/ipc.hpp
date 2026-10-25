/** @file
 * @brief Asynchronous message passing between threads, and notifications
 * from the kernel.
 */
#ifndef CORE_IPC_HPP
#define CORE_IPC_HPP

#include <cock/ipc/message.hpp>
#include <stdint.h>

namespace cock::core::task {

using ipc::Message;

/**
 * @brief Core Inter-Process Communication (IPC) manager.
 * * Handles the safe delivery and retrieval of messages between threads,
 * modifying
 * * thread states and invoking the scheduler when necessary.
 *
 * Delivery blocks interrupts while it runs, so it may be used from interrupt
 * context. A target waiting for a message, or waiting in a synchronous call,
 * is woken.
 */
class IPC {
  public:
	/**
	 * @brief Sends a message to the target thread's inbox.
	 * * If the target thread is sleeping (WAITING_MSG), it is awakened and
	 * placed in the ready queue.
	 * @param to_pid The Process ID of the target thread.
	 * @param message The message structure to deliver.
	 * @note On success the sender yields. Failure is silent, use deliver to
	 * learn it. Process context only, since it yields.
	 */
	static void send(uint32_t to_pid, Message message);

	/**
	 * @brief Delivers a message without yielding.
	 * @param to_pid Thread id of the target.
	 * @param message Message to deliver. The target field is overwritten by the
	 * kernel; `badge` and `reply` are the caller's to set.
	 * @return True when queued. False when the target does not exist, is dead
	 * or its inbox is full.
	 */
	static bool deliver(uint32_t to_pid, Message message);
	/**
	 * @brief Raises an interrupt notification on a thread.
	 * @param to_pid Thread id of the target.
	 * @param line Interrupt line recorded in the notification.
	 * @return True when raised. False when the target does not exist or is
	 * dead.
	 * @note Notifications do not use the inbox, so they cannot be lost to a
	 * full queue. Called from interrupt context.
	 */
	static bool notify(uint32_t to_pid, uint32_t line);
	/**
	 * @brief Records the exit of a child in the process record of its parent and
	 * wakes the main thread of the parent when it waits for a message.
	 * @param parent_pid Process id of the parent, which is the id of its main
	 * thread.
	 * @param child_pid Process id of the child.
	 * @param status Exit status of the child.
	 * @return True when recorded. False when no process has that id, or the
	 * main thread of the parent is already reaped.
	 * @note The record is not the inbox, so a full inbox cannot lose the notice.
	 * Only the main thread of the parent reads it, as a `PROCESS_EXITED`
	 * message sent by the kernel.
	 */
	static bool notifyExit(uint32_t parent_pid, uint32_t child_pid, uint32_t status);

	/**
	 * @brief Attempts to retrieve a message from the current thread's inbox.
	 * * If the inbox is empty, the thread's state is changed to
	 * BLOCKED/WAITING_MSG
	 * * and it yields the CPU until a message arrives.
	 * @param message Pointer to store the retrieved message.
	 * @note Kernel threads only. Does nothing for a null pointer or when there
	 * is no current thread. Does not return until a message was copied.
	 */
	static void receive(Message *message);
};

} // namespace cock::core::task

#endif // !CORE_IPC_HPP
