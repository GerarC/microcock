#include <ata/reply.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

using cock::abi::block::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::ata {

void reply(uint32_t pid, uint32_t id, Status status, uint32_t first, uint32_t second,
		   uint32_t third) {
	Message answer = {};
	answer.type = MessageType::BLK_REPLY;
	answer.payload.args.arg0 = id;
	answer.payload.args.arg1 = static_cast<uint32_t>(status);
	answer.payload.args.arg2 = first;
	answer.payload.args.arg3 = second;
	answer.payload.args.arg4 = third;
	cock::user::ipc_send(pid, answer);
}

} // namespace cock::ata
