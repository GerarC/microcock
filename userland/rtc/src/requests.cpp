#include <cock/abi/datetime.hpp>
#include <cock/abi/rtc.hpp>
#include <cock/user/cmos.hpp>
#include <rtc/reply.hpp>
#include <rtc/requests.hpp>
#include <stdint.h>

using cock::abi::rtc::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::rtc {

namespace {

cock::user::Cmos cmos;

} // namespace

bool initClock() { return cmos.init(); }

void handle(const Message &message) {
	if (message.sender_pid == cock::ipc::KERNEL_SENDER) return;
	if (message.type != MessageType::RTC_READ) return;

	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	cock::abi::datetime::DateTime now = {};
	if (!cmos.now(now)) return reply(pid, id, Status::UNAVAILABLE);
	reply(pid, id, Status::OK, cock::abi::rtc::packDate(now), cock::abi::rtc::packTime(now));
}

} // namespace cock::rtc
