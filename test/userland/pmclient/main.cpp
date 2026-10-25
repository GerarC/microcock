#include <cock/abi/pm.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <cock/user/pm.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::pm::State;
using cock::abi::pm::Status;
using cock::user::pm::Listing;

namespace {

constexpr char SERVER_NAME[] = "pm";
constexpr uint32_t FIRST_ROW = 0;
constexpr size_t FIRST_PROCESSES = 5;
constexpr size_t NO_ROWS = 0;
constexpr size_t SOME_IMAGE_BYTES = 0x1000;
constexpr uint32_t SOME_PROCESS = 1;
constexpr uint32_t SOME_CHILD = 2;
constexpr char SOME_NAME[] = "stranger";
constexpr uint32_t NO_FLAGS = 0;
constexpr uint32_t NO_KERNEL_PROCESS = cock::user::NO_PROCESS;
constexpr uintptr_t EXIT_PASSED = 0;
constexpr uintptr_t EXIT_FAILED = 1;

struct Census {
	size_t rows = NO_ROWS;
	size_t other = NO_ROWS;
	bool rootFirst = false;
	bool selfSleeping = false;
	bool selfChildOfRoot = false;
};

bool isLive(State state) {
	return state == State::RUNNING || state == State::SLEEPING;
}

Census count(cock::user::pm::Client &client) {
	Census census;
	uint32_t rootPid = cock::abi::pm::NO_PID;
	uint32_t self = cock::user::self();
	for (uint32_t index = FIRST_ROW;; index++) {
		Listing row = client.list(index);
		if (row.status != Status::OK) break;
		census.rows++;
		if (index == FIRST_ROW) {
			rootPid = row.pid;
			census.rootFirst = row.kernelId == cock::abi::service::ROOT_PID &&
							   row.parent == cock::abi::pm::NO_PID && isLive(row.state);
		}
		if (!isLive(row.state) && row.state != State::EXITED) census.other++;
		if (row.kernelId == self) {
			census.selfSleeping = row.state == State::SLEEPING;
			census.selfChildOfRoot = row.parent == rootPid;
		}
	}
	return census;
}

bool refusesStrangers(cock::user::pm::Client &client) {
	uint32_t pid = cock::abi::pm::NO_PID;
	bool registered = client.registerProcess(SOME_PROCESS, cock::abi::pm::NO_PARENT, cock::abi::pm::NO_PID, pid);
	bool room = client.begin(SOME_IMAGE_BYTES) != nullptr;
	cock::abi::service::SpawnRights rights = {};
	uint32_t spawned = room ? client.finish(SOME_NAME, rights, NO_FLAGS, SOME_IMAGE_BYTES) : NO_KERNEL_PROCESS;
	bool denied = client.lastStatus() == Status::DENIED;
	client.release();
	return !registered && room && spawned == NO_KERNEL_PROCESS && denied;
}

bool hasNoChildren(cock::user::pm::Client &client) {
	cock::user::pm::Waited any = client.wait();
	cock::user::pm::Waited one = client.wait(SOME_CHILD);
	return any.status == Status::NOT_FOUND && one.status == Status::NOT_FOUND;
}

} // namespace

extern "C" int main() {
	cock::user::names::Connection pm = cock::user::names::connect(SERVER_NAME);
	if (pm.status != cock::abi::names::Status::OK) {
		puts("pmclient: cannot reach pm");
		return EXIT_FAILED;
	}

	cock::user::pm::Client client(pm.handle);
	Census census = count(client);
	bool ok = census.rows >= FIRST_PROCESSES && census.rootFirst && census.selfSleeping &&
			  census.selfChildOfRoot && census.other == NO_ROWS &&
			  refusesStrangers(client) && hasNoChildren(client);

	puts(ok ? "pmclient: ok" : "pmclient: wrong table");
	return ok ? EXIT_PASSED : EXIT_FAILED;
}
