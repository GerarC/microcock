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
 */
class IPC {
  public:
	/**
	 * @brief Sends a message to the target thread's inbox.
	 * * If the target thread is sleeping (WAITING_MSG), it is awakened and
	 * placed in the ready queue.
	 * @param to_pid The Process ID of the target thread.
	 * @param message The message structure to deliver.
	 */
	static void send(uint32_t to_pid, Message message);

	/**
	 * @brief Attempts to retrieve a message from the current thread's inbox.
	 * * If the inbox is empty, the thread's state is changed to
	 * BLOCKED/WAITING_MSG
	 * * and it yields the CPU until a message arrives.
	 * @param message Pointer to store the retrieved message.
	 */
	static void receive(Message *message);
};

} // namespace cock::core::task

#endif // !CORE_IPC_HPP
