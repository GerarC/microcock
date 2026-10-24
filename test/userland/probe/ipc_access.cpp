#include "probe.hpp"
#include <cock/abi/block.hpp>
#include <cock/abi/console.hpp>
#include <cock/abi/names.hpp>
#include <cock/abi/rtc.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/joinable_thread.hpp>
#include <cock/user/names.hpp>
#include <stddef.h>
#include <stdint.h>

using cock::abi::names::Status;
using cock::user::JoinableThread;
using cock::user::CALL_WORDS;
using cock::user::NO_PROCESS;

namespace probe {

namespace {

constexpr size_t SHARED_PAGES = 1;
constexpr uint32_t UNKNOWN_OPERATION = 0xFF;
constexpr size_t NO_LENGTH = 0;
constexpr uint32_t FIRST_SCANNED_PID = 1;
constexpr uint32_t LAST_SCANNED_PID = 63;
constexpr size_t LEAST_REFUSALS = 3;
constexpr size_t CONNECT_ROUNDS = 40;
constexpr uint32_t NOBODY_GAVE_OFFSET = 1000;
constexpr uint32_t FORGED_LABEL = 5;
constexpr const char UNKNOWN_SERVER[] = "nothing";
constexpr const char TOO_LONG_SERVER[] =
	"a_name_that_is_longer_than_sixty_bytes_because_it_goes_on_and_on_and_on_and_on";
constexpr const char PATH_SERVER[] = "/run/console";
constexpr const char LONG_UNKNOWN_SERVER[] = "/run/a_server_with_a_long_name_that_nobody_runs";
constexpr size_t STACK_BYTES = 0x4000;
constexpr uint32_t THREAD_DEADLINE_MILLISECONDS = 3000;

alignas(PAGE_BYTES) uint8_t connectStack[STACK_BYTES];
JoinableThread connectThread;
uint32_t pidSeenByThread = NO_PROCESS;

void connectFromThread(void *) {
	pidSeenByThread = cock::user::names::server(cock::abi::console::SERVER_NAME);
}

}

bool connectRefusesWhatThePolicyDoesNotList() {
	return cock::user::names::connect(cock::abi::block::SERVER_NAME).status == Status::DENIED &&
		   cock::user::names::connect(cock::abi::rtc::SERVER_NAME).status == Status::DENIED &&
		   cock::user::names::connect(cock::abi::block::SERVER_NAME).pid == NO_PROCESS &&
		   cock::user::names::connect(UNKNOWN_SERVER).status == Status::NOT_FOUND &&
		   cock::user::names::connect(TOO_LONG_SERVER).status == Status::BAD_REQUEST;
}

bool unconnectedServersRefuseMessages() {
	size_t refused = 0;
	cock::ipc::Message message = {};
	for (uint32_t pid = FIRST_SCANNED_PID; pid <= LAST_SCANNED_PID; pid++) {
		if (pid == cock::user::self() || !cock::user::process_alive(pid)) continue;
		if (!cock::user::ipc_send(pid, message)) refused++;
	}
	return refused >= LEAST_REFUSALS;
}

bool connectGivesTheRightToTalk() {
	cock::user::names::Connection connection =
		cock::user::names::connect(cock::abi::console::SERVER_NAME);
	if (connection.status != Status::OK || connection.pid == NO_PROCESS) return false;

	const uint32_t words[CALL_WORDS] = {
		cock::abi::console::pack(UNKNOWN_OPERATION, NO_LENGTH), 0, 0, 0};
	cock::user::CallResult answer = cock::user::ipc_call(connection.pid, words);
	return answer.ok &&
		   (answer.words[cock::abi::console::HEADER_WORD] & cock::abi::console::BYTE_MASK) ==
			   static_cast<uint32_t>(cock::abi::console::Status::BAD_REQUEST);
}

bool registrationIsRootOnly() {
	constexpr cock::abi::names::Op OPERATIONS[] = {cock::abi::names::Op::DEFINE,
												   cock::abi::names::Op::ALLOW,
												   cock::abi::names::Op::BIND};
	for (cock::abi::names::Op operation : OPERATIONS) {
		const uint32_t words[CALL_WORDS] = {cock::abi::names::pack(operation, FORGED_LABEL),
											FORGED_LABEL, 0, 0};
		cock::user::CallResult answer = cock::user::ipc_call(cock::abi::service::NAMES_PID, words);
		if (!answer.ok ||
			static_cast<Status>(answer.words[cock::abi::names::HEADER_WORD]) != Status::NOT_ROOT)
			return false;
	}
	return true;
}

bool connectAcceptsPathsAndLongNames() {
	uint32_t console = cock::user::names::server(cock::abi::console::SERVER_NAME);
	return console != NO_PROCESS && cock::user::names::server(PATH_SERVER) == console &&
		   cock::user::names::connect(LONG_UNKNOWN_SERVER).status == Status::NOT_FOUND;
}

bool connectWorksFromAnotherThread() {
	uint32_t console = cock::user::names::server(cock::abi::console::SERVER_NAME);
	pidSeenByThread = NO_PROCESS;
	if (console == NO_PROCESS ||
		!connectThread.start(connectFromThread, nullptr, connectStack, STACK_BYTES))
		return false;
	return connectThread.join(THREAD_DEADLINE_MILLISECONDS) && pidSeenByThread == console;
}

bool connectingTwiceUsesNoNewCapability() {
	for (size_t round = 0; round < CONNECT_ROUNDS; round++) {
		cock::user::names::Connection connection =
			cock::user::names::connect(cock::abi::console::SERVER_NAME);
		if (connection.status != Status::OK) return false;
	}
	return true;
}

bool connectGivesAHandleOnTheEndpoint() {
	cock::user::names::Connection first =
		cock::user::names::connect(cock::abi::console::SERVER_NAME);
	cock::user::names::Connection second =
		cock::user::names::connect(cock::abi::console::SERVER_NAME);
	if (first.status != Status::OK || first.handle == cock::abi::NO_CAPABILITY_NODE ||
		second.handle != first.handle)
		return false;

	const uint32_t words[CALL_WORDS] = {
		cock::abi::console::pack(UNKNOWN_OPERATION, NO_LENGTH), 0, 0, 0};
	cock::user::CallResult answer = cock::user::ep_call(first.handle, words);
	bool reached = answer.ok &&
				   (answer.words[cock::abi::console::HEADER_WORD] & cock::abi::console::BYTE_MASK) ==
					   static_cast<uint32_t>(cock::abi::console::Status::BAD_REQUEST);

	uint32_t unused = 0;
	return reached &&
		   cock::user::ep_call(first.handle + NOBODY_GAVE_OFFSET, words).status ==
			   cock::user::CallStatus::FAILED &&
		   cock::user::ep_create(unused) == cock::abi::endpoint::NO_ENDPOINT &&
		   !cock::user::ep_bind(cock::abi::endpoint::FIRST_ENDPOINT, cock::user::self());
}

bool sharedMemoryNeedsConnect() {
	uint32_t object = cock::user::shm_create(SHARED_PAGES);
	if (object == cock::user::NO_SHM) return false;

	uint32_t console = cock::user::names::server(cock::abi::console::SERVER_NAME);
	bool ok = console != NO_PROCESS && cock::user::shm_share(object, console, false);
	return cock::user::shm_release(object) && ok;
}

} // namespace probe
