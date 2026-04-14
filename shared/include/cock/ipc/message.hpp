#ifndef SHARED_COCK_IPC_HPP
#define SHARED_COCK_IPC_HPP

#include <stddef.h>
#include <stdint.h>
namespace cock::ipc {

/**
 * @brief Maximum number of arguments a standard IPC message can carry.
 */
static constexpr size_t MAX_ARGUMENTS = 6;

/**
 * @brief Defines the classification of IPC messages used for routing and handling.
 */
enum class MessageType : uint32_t {
	NONE = 0,                       /**< Null or empty message. */
	SYS_PING = 1,                   /**< System ping for heartbeat or test purposes. */
	SYS_PONG = 2,                   /**< System pong response. */
	SYS_DEBUG = 3,                  /**< Debugging message for the kernel logger. */
	VIDEO_WRITE_CHAR = 4,           /**< Request to write a single character to the Video server. */
	VIDEO_WRITE_STR = 5,            /**< Request to write a string to the Video server. */
	SYS_REGISTER_MAILBOX = 6,       /**< System request to register a shared memory mailbox. */
};

/**
 * @brief Standard Inter-Process Communication (IPC) Message structure.
 * * Used to pass data asynchronously between isolated Userland processes and the Kernel.
 */
typedef struct message_t {
	uint32_t sender_pid;            /**< Process ID of the sender. */
	uint32_t target_pid;            /**< Process ID of the intended recipient. */
	MessageType type;               /**< The type/command of the message. */

    /**
     * @brief Payload carrying the actual data of the message.
     */
	union {
		uint32_t argv[MAX_ARGUMENTS]; /**< Array access to arguments. */
		struct {
			uint32_t arg0;
			uint32_t arg1;
			uint32_t arg2;
			uint32_t arg3;
			uint32_t arg4;
			uint32_t arg5;
		} args;                       /**< Named access to arguments. */
		void *ptr;                    /**< Pointer access for shared memory/mailbox addresses. */
	} payload;

} Message;

} // namespace cock::ipc

#endif // !SHARED_COCK_IPC_HPP
