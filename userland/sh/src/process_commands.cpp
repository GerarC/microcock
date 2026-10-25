#include <cock/abi/pm.hpp>
#include <cock/user/names.hpp>
#include <cock/user/pm.hpp>
#include <sh/process_commands.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::abi::pm::State;
using cock::abi::pm::Status;

namespace cock::sh {

namespace {

constexpr char SERVER_NAME[] = "pm";
constexpr uint32_t FIRST_ROW = 0;
constexpr char STATE_RUNNING = 'R';
constexpr char STATE_EXITED = 'X';
constexpr char STATE_UNKNOWN = '?';

cock::user::pm::Client client;
bool connected = false;

bool connect() {
	if (connected) return true;
	cock::user::names::Connection connection = cock::user::names::connect(SERVER_NAME);
	if (connection.status != cock::abi::names::Status::OK) return false;
	client.connect(connection.handle);
	connected = true;
	return true;
}

char stateLetter(State state) {
	if (state == State::RUNNING) return STATE_RUNNING;
	if (state == State::EXITED) return STATE_EXITED;
	return STATE_UNKNOWN;
}

} // namespace

void processListCommand(const cock::shell::Words &) {
	if (!connect()) {
		printf("sh: ps: pm does not answer\n");
		return;
	}

	printf("PID PPID STAT\n");
	for (uint32_t index = FIRST_ROW;; index++) {
		cock::user::pm::Listing row = client.list(index);
		if (row.status == Status::NOT_FOUND) return;
		if (row.status != Status::OK) {
			connected = false;
			printf("sh: ps: pm does not answer\n");
			return;
		}
		if (row.parent == cock::abi::pm::NO_PID)
			printf("%u - %c\n", row.pid, stateLetter(row.state));
		else
			printf("%u %u %c\n", row.pid, row.parent, stateLetter(row.state));
	}
}

} // namespace cock::sh
