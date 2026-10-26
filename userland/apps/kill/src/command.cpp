#include <cock/abi/process.hpp>
#include <cock/user/number.hpp>
#include <cock/user/pm.hpp>
#include <cock/user/tool.hpp>
#include <kill/command.hpp>
#include <kill/manager.hpp>
#include <kill/outcome.hpp>
#include <kill/signal.hpp>
#include <stdint.h>
#include <stdio.h>

using namespace cock::user::tool;

namespace cock::kill {

namespace {

constexpr char USAGE_ARGUMENTS[] = "[-signal] pid...";

int usageError(const char *program) {
	return usage(program, USAGE_ARGUMENTS);
}

} // namespace

int run(int argc, char **argv) {
	int first = FIRST_ARGUMENT;
	uint32_t signal = cock::abi::process::SIGNAL_TERMINATE;
	if (argumentCount(argc) > NO_ARGUMENTS && argv[first][0] == SIGNAL_PREFIX) {
		if (!parseSignal(argv[first] + ONE_ARGUMENT, signal)) return usageError(argv[0]);
		first++;
	}
	if (first >= argc) return usageError(argv[0]);

	uint32_t pid = 0;
	for (int index = first; index < argc; index++)
		if (!parseUnsigned(argv[index], pid)) return usageError(argv[0]);

	cock::user::pm::Client client;
	if (!connectManager(client)) {
		printf("%s: pm does not answer\n", argv[0]);
		return STATUS_FAILURE;
	}

	int status = STATUS_OK;
	for (int index = first; index < argc; index++) {
		parseUnsigned(argv[index], pid);
		cock::abi::pm::Status answer = client.kill(pid, signal);
		if (answer == cock::abi::pm::Status::OK) continue;
		printf("%s: %s: %s\n", argv[0], argv[index], describe(answer));
		status = STATUS_FAILURE;
	}
	return status;
}

} // namespace cock::kill
