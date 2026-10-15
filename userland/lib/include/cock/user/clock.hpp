#ifndef COCK_USER_CLOCK_HPP
#define COCK_USER_CLOCK_HPP

#include <cock/abi/datetime.hpp>
#include <cock/abi/rtc.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/reply.hpp>
#include <stdint.h>

namespace cock::user {

class Clock {
  public:
	void (*onForeign)(const ipc::Message &) = nullptr;

	bool init() {
		server = lookup(abi::rtc::SERVER_NAME);
		return server != NO_PROCESS;
	}

	bool now(abi::datetime::DateTime &time) {
		if (server == NO_PROCESS) return false;

		uint32_t id = nextId++;
		ipc::Message request = {};
		request.type = ipc::MessageType::RTC_READ;
		request.payload.args.arg0 = id;
		if (!ipc_send(server, request)) return false;

		ipc::Message answer = {};
		bool matches = receiveReply(
			server, answer,
			[&](const ipc::Message &candidate) {
				return candidate.type == ipc::MessageType::RTC_REPLY &&
					   candidate.sender_pid == server && candidate.payload.args.arg0 == id;
			},
			onForeign);
		if (!matches ||
			static_cast<abi::rtc::Status>(answer.payload.args.arg1) != abi::rtc::Status::OK)
			return false;

		return abi::rtc::unpack(answer.payload.args.arg2, answer.payload.args.arg3, time);
	}

  private:
	uint32_t server = NO_PROCESS;
	uint32_t nextId = 1;
};

} // namespace cock::user

#endif // !COCK_USER_CLOCK_HPP
