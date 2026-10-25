#include <cock/abi/datetime.hpp>
#include <cock/abi/endpoint.hpp>
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
	if (message.reply == cock::abi::endpoint::NO_REPLY) return;
	if (message.type != MessageType::RTC_READ) return;

	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	cock::abi::datetime::DateTime now = {};
	if (!cmos.now(now)) return reply(token, id, Status::UNAVAILABLE);
	reply(token, id, Status::OK, cock::abi::rtc::packDate(now), cock::abi::rtc::packTime(now));
}

} // namespace cock::rtc
