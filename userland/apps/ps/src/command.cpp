#include <cock/abi/pm.hpp>
#include <cock/user/pm.hpp>
#include <cock/user/tool.hpp>
#include <ps/command.hpp>
#include <ps/manager.hpp>
#include <ps/state.hpp>
#include <stdint.h>
#include <stdio.h>

using cock::abi::pm::Status;
using namespace cock::user::tool;

namespace cock::ps {

namespace {

constexpr uint32_t FIRST_ROW = 0;

int unreachable(const char *program) {
	printf("%s: pm does not answer\n", program);
	return STATUS_FAILURE;
}

} // namespace

int run(int, char **argv) {
	cock::user::pm::Client client;
	if (!connectManager(client)) return unreachable(argv[0]);

	printf("PID PPID STAT\n");
	for (uint32_t index = FIRST_ROW;; index++) {
		cock::user::pm::Listing row = client.list(index);
		if (row.status == Status::NOT_FOUND) return STATUS_OK;
		if (row.status != Status::OK) return unreachable(argv[0]);

		if (row.parent == cock::abi::pm::NO_PID)
			printf("%u - %c\n", row.pid, stateLetter(row.state));
		else
			printf("%u %u %c\n", row.pid, row.parent, stateLetter(row.state));
	}
}

} // namespace cock::ps
