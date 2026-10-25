#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <rtc/reply.hpp>

using cock::abi::rtc::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::rtc {

void reply(uint32_t token, uint32_t id, Status status, uint32_t date, uint32_t time) {
	Message answer = {};
	answer.type = MessageType::RTC_REPLY;
	answer.payload.args.arg0 = id;
	answer.payload.args.arg1 = static_cast<uint32_t>(status);
	answer.payload.args.arg2 = date;
	answer.payload.args.arg3 = time;
	cock::user::ep_reply(token, answer);
}

} // namespace cock::rtc
