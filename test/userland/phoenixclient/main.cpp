#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;

namespace {

constexpr char SERVER_NAME[] = "phoenix";
constexpr uint32_t ROUNDS = 3;
constexpr uint32_t ATTEMPTS = 200;
constexpr uint32_t ATTEMPT_MILLISECONDS = 50;
constexpr uintptr_t EXIT_RESTARTED = 0;
constexpr uintptr_t EXIT_NOT_RESTARTED = 1;

bool askOnce(uint32_t server) {
	Message ping = {};
	ping.type = MessageType::SYS_PING;
	if (!cock::user::ipc_send(server, ping)) return false;

	Message reply = {};
	for (uint32_t waited = 0; waited < ATTEMPTS; waited++) {
		cock::user::Received received =
			cock::user::ipc_recv_timeout(reply, ATTEMPT_MILLISECONDS);
		if (received == cock::user::Received::MESSAGE)
			return reply.type == MessageType::SYS_PONG && reply.sender_pid == server;
		if (received == cock::user::Received::FAILED || !cock::user::process_alive(server))
			return false;
	}
	return false;
}

uint32_t findNewServer(uint32_t previous) {
	for (uint32_t attempt = 0; attempt < ATTEMPTS; attempt++) {
		uint32_t server = cock::user::lookup(SERVER_NAME);
		if (server != NO_PROCESS && server != previous && cock::user::process_alive(server))
			return server;
		cock::user::sleep_ms(ATTEMPT_MILLISECONDS);
	}
	return NO_PROCESS;
}

} // namespace

extern "C" int main() {
	uint32_t previous = NO_PROCESS;
	uint32_t answered = 0;
	for (uint32_t round = 0; round < ROUNDS; round++) {
		uint32_t server = findNewServer(previous);
		if (server == NO_PROCESS || !askOnce(server)) break;
		previous = server;
		answered++;
	}

	bool restarted = answered == ROUNDS;
	puts(restarted ? "phoenixclient: ok" : "phoenixclient: not restarted");
	cock::user::exit_process(restarted ? EXIT_RESTARTED : EXIT_NOT_RESTARTED);
}
