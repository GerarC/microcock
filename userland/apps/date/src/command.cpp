#include <cock/abi/datetime.hpp>
#include <cock/user/clock.hpp>
#include <cock/user/tool.hpp>
#include <date/command.hpp>
#include <date/format.hpp>
#include <stdio.h>

using namespace cock::user::tool;

namespace cock::date {

namespace {

constexpr char NO_CLOCK_MESSAGE[] = "date: clock: not available\n";

}

int run(int argc, char **argv) {
	if (argumentCount(argc) != NO_ARGUMENTS) return usage(argv[0], "");

	cock::user::Clock clock;
	cock::abi::datetime::DateTime now = {};
	if (!clock.init() || !clock.now(now)) {
		printf(NO_CLOCK_MESSAGE);
		return STATUS_FAILURE;
	}

	char text[TEXT_BYTES] = {};
	formatDateTime(now, text);
	printf("%s\n", text);
	return STATUS_OK;
}

} // namespace cock::date
