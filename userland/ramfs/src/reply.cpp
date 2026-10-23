#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <ramfs/reply.hpp>

using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::ramfs {

void reply(uint32_t pid, uint32_t id, Status status, uint32_t first, uint32_t second) {
	Message answer = {};
	answer.type = MessageType::FS_REPLY;
	answer.payload.args.arg0 = id;
	answer.payload.args.arg1 = static_cast<uint32_t>(status);
	answer.payload.args.arg2 = first;
	answer.payload.args.arg3 = second;
	cock::user::ipc_send(pid, answer);
}

} // namespace cock::ramfs
