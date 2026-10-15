#ifndef SHARED_COCK_IPC_HPP
#define SHARED_COCK_IPC_HPP

#include <stddef.h>
#include <stdint.h>
namespace cock::ipc {

/**
 * @brief Maximum number of arguments a standard IPC message can carry.
 */
static constexpr size_t MAX_ARGUMENTS = 6;

static constexpr uint32_t KERNEL_SENDER = 0;

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
	IRQ_NOTIFICATION = 7,           /**< Kernel notice that a bound hardware interrupt fired; arg0 is the line. */
	CAPABILITY_GRANTED = 8,         /**< Root server notice that the capabilities were granted; arg0 is a peer process id. */
	FS_CONNECT = 9,                 /**< File server request: open a session and get a window; arg0 is the request id. */
	FS_OPEN = 10,                   /**< File server request: open the path in the window; arg0 request id, arg1 path length, arg2 flags. */
	FS_READ = 11,                   /**< File server request: read into the window; arg0 request id, arg1 handle, arg2 offset, arg3 length. */
	FS_STAT = 12,                   /**< File server request: size and type of a handle; arg0 request id, arg1 handle. */
	FS_CLOSE = 13,                  /**< File server request: drop a handle; arg0 request id, arg1 handle. */
	FS_READDIR = 15,                /**< File server request: name of the entry at an index of a directory; arg0 request id, arg1 handle, arg2 index. */
	FS_REPLY = 14,                  /**< File server answer; arg0 request id, arg1 status, arg2 and arg3 values. */
	BLK_CONNECT = 16,               /**< Block server request: open a session and get a window; arg0 is the request id. */
	BLK_INFO = 17,                  /**< Block server request: size of a device; arg0 request id, arg1 device. */
	BLK_READ = 18,                  /**< Block server request: read sectors into the window; arg0 request id, arg1 device, arg2 first sector, arg3 sector count. */
	BLK_REPLY = 19,                 /**< Block server answer; arg0 request id, arg1 status, arg2 to arg4 values. */
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
