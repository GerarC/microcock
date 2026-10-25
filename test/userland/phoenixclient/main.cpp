#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
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

bool askOnce(const cock::user::names::Connection &server) {
	Message ping = {};
	ping.type = MessageType::SYS_PING;
	if (!cock::user::ep_send(server.handle, ping, true)) return false;

	Message reply = {};
	for (uint32_t waited = 0; waited < ATTEMPTS; waited++) {
		cock::user::Received received =
			cock::user::ipc_recv_timeout(reply, ATTEMPT_MILLISECONDS);
		if (received == cock::user::Received::MESSAGE)
			return reply.type == MessageType::SYS_PONG &&
				   reply.badge == cock::abi::endpoint::NO_BADGE;
		if (received == cock::user::Received::FAILED || !cock::user::process_alive(server.pid))
			return false;
	}
	return false;
}

cock::user::names::Connection findNewServer(uint32_t previous) {
	cock::user::names::Connection connection = {};
	for (uint32_t attempt = 0; attempt < ATTEMPTS; attempt++) {
		connection = cock::user::names::connect(SERVER_NAME);
		if (connection.pid != NO_PROCESS && connection.pid != previous &&
			cock::user::process_alive(connection.pid))
			return connection;
		cock::user::sleep_ms(ATTEMPT_MILLISECONDS);
	}
	connection.pid = NO_PROCESS;
	return connection;
}

} // namespace

extern "C" int main() {
	uint32_t previous = NO_PROCESS;
	uint32_t answered = 0;
	for (uint32_t round = 0; round < ROUNDS; round++) {
		cock::user::names::Connection server = findNewServer(previous);
		if (server.pid == NO_PROCESS || !askOnce(server)) break;
		previous = server.pid;
		answered++;
	}

	bool restarted = answered == ROUNDS;
	puts(restarted ? "phoenixclient: ok" : "phoenixclient: not restarted");
	cock::user::exit_process(restarted ? EXIT_RESTARTED : EXIT_NOT_RESTARTED);
}
