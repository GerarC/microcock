/**
 * @file clock.hpp
 * @brief Client of the RTC server: reads the wall clock without touching the CMOS ports.
 *
 * Talks to the RTC server by asynchronous messages (`RTC_READ` and `RTC_REPLY`). The
 * server is the only process that holds the CMOS ports (see `cmos.hpp`).
 */
#ifndef COCK_USER_CLOCK_HPP
#define COCK_USER_CLOCK_HPP

#include <cock/abi/datetime.hpp>
#include <cock/abi/rtc.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/reply.hpp>
#include <stdint.h>

namespace cock::user {

/** Connection to the RTC server. Not thread safe. */
class Clock {
  public:
	/**
	 * Called with every message that arrives while a reply is awaited and is not that
	 * reply. When null such messages are dropped.
	 */
	void (*onForeign)(const ipc::Message &) = nullptr;

	/**
	 * @brief Finds the RTC server by name (`SYS_PROC_LOOKUP`).
	 * @return true when the server exists; false otherwise (call again later).
	 */
	bool init() {
		server = lookup(abi::rtc::SERVER_NAME);
		return server != NO_PROCESS;
	}

	/**
	 * @brief Reads the current date and time (`RTC_READ`). Blocks until the server answers.
	 * @param time Receives the date and time on success.
	 * @return true on success; false when `init` did not succeed, the send failed, the
	 *         server died, reported an error, or sent a value that is not a valid date.
	 */
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
	/** Process id of the RTC server, `NO_PROCESS` before `init`. */
	uint32_t server = NO_PROCESS;
	/** Id of the next request. */
	uint32_t nextId = 1;
};

} // namespace cock::user

#endif // !COCK_USER_CLOCK_HPP
