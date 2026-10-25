/**
 * @file message.hpp
 * @brief Asynchronous IPC message (`SYS_EP_SEND`, `SYS_IPC_SEND`, `SYS_IPC_RECV`) and the catalog of message types.
 *
 * A message is a fixed 40 byte record: target, type, reply token, badge and a payload of six 32 bit words.
 * The kernel overwrites `badge` with the badge of the handle used and `reply` with a token it opened (or none),
 * so receivers can trust both: who sent a message is the badge, never a process id the sender wrote. Request and
 * reply protocols are described with the ABI header of each server (`fs.hpp`, `block.hpp`, `console.hpp`,
 * `rtc.hpp`).
 */
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
	OUTPUT_WRITE_CHAR = 4,           /**< Request to write a single character to the output server (the process that owns the screen). */
	OUTPUT_WRITE_STR = 5,            /**< Request to write a string to the output server (the process that owns the screen). */
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
	FS_WRITE = 20,                  /**< File server request: write the window into a file; arg0 request id, arg1 handle, arg2 offset, arg3 length. */
	FS_UNLINK = 21,                 /**< File server request: delete the file at the path in the window; arg0 request id, arg1 path length. */
	BLK_WRITE = 22,                 /**< Block server request: write the window to sectors; arg0 request id, arg1 device, arg2 first sector, arg3 sector count. */
	FS_MKDIR = 23,                  /**< File server request: make the directory at the path in the window; arg0 request id, arg1 path length. */
	FS_RMDIR = 24,                  /**< File server request: remove the empty directory at the path in the window; arg0 request id, arg1 path length. */
	FS_RENAME = 25,                 /**< File server request: rename; the old path is at the start of the window and the new one at RENAME_NEW_OFFSET; arg0 request id, arg1 old length, arg2 new length. */
	RTC_READ = 27,                  /**< Clock server request: read the date and time; arg0 is the request id. */
	RTC_REPLY = 28,                 /**< Clock server answer; arg0 request id, arg1 status, arg2 packed date, arg3 packed time. */
	PROCESS_EXITED = 29,            /**< Kernel notice to the spawner, only when it asked at spawn: arg0 is the process id, arg1 the exit status (FAULT_STATUS when an exception ended it). */
	CONSOLE_KEY = 30,               /**< Keyboard driver notice to the console server: arg0 is the typed character. */
	CAPABILITY_TRANSFER = 31,       /**< A capability the sender holds, given to the receiver by the kernel when the message is sent; arg0 capability type, arg1 first, arg2 count, arg3 flags (a combination of GRANT_DELEGATE and GRANT_MINT), arg4 is overwritten with the node id the receiver now holds, arg5 is the badge of an `ENDPOINT` capability (zero unless the sender holds the mint right). Refused unless the receiver accepted the type with SYS_CAP_ACCEPT. */
	PM_EXITED = 32,                 /**< Process manager notice to the root server for a process that asked for it with `FORWARD_EXIT`: arg0 is the kernel process id, arg1 the exit status as in `PROCESS_EXITED`. The kernel stamps the badge of the handle, which is the process id of `pm`; the root server ignores any other. */
	PM_SPAWNED = 33,                /**< Process manager notice to the root server for a process that `init` asked for: arg0 is the kernel process id, arg1 the service label it was created with. The badge is the process id of `pm`; the root server ignores any other. */
	PM_FORKED = 34,                 /**< Process manager notice to the root server for a process created by `fork`: arg0 is the kernel process id of the copy, arg1 the kernel process id of the process it copies. The root server tells the name server, so the copy has the rights of its parent. The badge is the process id of `pm`; the root server ignores any other. */
	FS_LINK = 26,                /**< File server request: make a hard link; the existing path is at the start of the window and the new name at RENAME_NEW_OFFSET; arg0 request id, arg1 existing length, arg2 new length. */
};

/**
 * @brief Standard Inter-Process Communication (IPC) Message structure.
 * * Used to pass data asynchronously between isolated Userland processes and the Kernel.
 */
typedef struct message_t {
	uint32_t target_pid;            /**< Id of the thread that receives the message, written by the kernel. */
	MessageType type;               /**< The type/command of the message. */
	uint32_t reply;                 /**< Reply token (`SYS_EP_REPLY`, `SYS_EP_FORWARD`) when the sender asked for an answer (`SEND_EXPECT_REPLY`), written by the kernel; `abi::endpoint::NO_REPLY` otherwise. Only the process the message was delivered to can use it, and only once. */
	uint32_t badge;                 /**< Badge of the handle the sender used (`SYS_EP_SEND`), written by the kernel; `abi::endpoint::NO_BADGE` for a message sent between the threads of one process, for an answer (`SYS_EP_REPLY`) and for kernel notices. A sender cannot choose it. */

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
