#include "probe.hpp"
#include <cock/abi/block.hpp>
#include <cock/abi/console.hpp>
#include <cock/abi/rtc.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <stddef.h>
#include <stdint.h>

using cock::user::CALL_WORDS;
using cock::user::NO_PROCESS;

namespace probe {

namespace {

constexpr size_t SHARED_PAGES = 1;
constexpr uint32_t UNKNOWN_OPERATION = 0xFF;
constexpr size_t NO_LENGTH = 0;
constexpr uint32_t REFUSAL_WAIT_MILLISECONDS = 200;

uint32_t liveServer(const char *name) {
	uint32_t pid = cock::user::lookup(name);
	return pid != NO_PROCESS && cock::user::process_alive(pid) ? pid : NO_PROCESS;
}

bool refusedBy(uint32_t server) {
	if (server == NO_PROCESS) return false;

	cock::ipc::Message message = {};
	const uint32_t words[CALL_WORDS] = {};
	return !cock::user::ipc_send(server, message) &&
		   cock::user::ipc_call(server, words, REFUSAL_WAIT_MILLISECONDS).status ==
			   cock::user::CallStatus::FAILED;
}

} // namespace

bool ipcWithoutCapabilityIsRefused() {
	return refusedBy(liveServer(cock::abi::block::SERVER_NAME)) &&
		   refusedBy(liveServer(cock::abi::rtc::SERVER_NAME));
}

bool ipcWithCapabilityIsAdmitted() {
	uint32_t console = liveServer(cock::abi::console::SERVER_NAME);
	if (console == NO_PROCESS) return false;

	const uint32_t words[CALL_WORDS] = {
		cock::abi::console::pack(UNKNOWN_OPERATION, NO_LENGTH), 0, 0, 0};
	cock::user::CallResult answer = cock::user::ipc_call(console, words);
	return answer.ok &&
		   (answer.words[cock::abi::console::HEADER_WORD] & cock::abi::console::BYTE_MASK) ==
			   static_cast<uint32_t>(cock::abi::console::Status::BAD_REQUEST);
}

bool sharedMemoryNeedsCapability() {
	uint32_t object = cock::user::shm_create(SHARED_PAGES);
	if (object == cock::user::NO_SHM) return false;

	uint32_t blocks = liveServer(cock::abi::block::SERVER_NAME);
	uint32_t console = liveServer(cock::abi::console::SERVER_NAME);
	bool ok = blocks != NO_PROCESS && console != NO_PROCESS &&
			  !cock::user::shm_share(object, blocks, false) &&
			  cock::user::shm_share(object, console, false);
	return cock::user::shm_release(object) && ok;
}

} // namespace probe
