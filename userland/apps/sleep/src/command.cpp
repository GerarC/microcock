#include <cock/shell/words.hpp>
#include <cock/user/api.hpp>
#include <cock/user/tool.hpp>
#include <sleep/command.hpp>

using namespace cock::user::tool;

namespace cock::sleep {

int run(int argc, char **argv) {
	uint32_t seconds = 0;
	if (argumentCount(argc) != ONE_ARGUMENT || !cock::shell::parseNumber(argv[FIRST_ARGUMENT], seconds) ||
		seconds > MAX_SECONDS)
		return usage(argv[0], "seconds");

	return cock::user::sleep_ms(seconds * MILLISECONDS_PER_SECOND) ? STATUS_OK : STATUS_FAILURE;
}

} // namespace cock::sleep
