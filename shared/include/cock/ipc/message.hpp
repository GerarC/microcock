#ifndef SHARED_COCK_IPC_HPP
#define SHARED_COCK_IPC_HPP

#include <stddef.h>
#include <stdint.h>
namespace cock::ipc {

static constexpr size_t MAX_ARGUMENTS = 5;
enum class MessageType : uint32_t {
	NONE = 0,
	SYS_PING = 1,
	SYS_PONG = 2,
	SYS_DEBUG = 2,
};

typedef struct message_t {
	uint32_t sender_pid;
	uint32_t target_pid;
    MessageType type;

	union {
		uint32_t argv[MAX_ARGUMENTS];
		struct {
			uint32_t arg1;
			uint32_t arg2;
			uint32_t arg3;
			uint32_t arg4;
			uint32_t arg5;
		} args;
		void *ptr;
	} payload;

} Message;

} // namespace cock::ipc

#endif // !SHARED_COCK_IPC_HPP
