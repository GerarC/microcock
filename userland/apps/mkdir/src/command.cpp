#include <cock/user/tool.hpp>
#include <mkdir/command.hpp>
#include <sys/stat.h>

using namespace cock::user::tool;

namespace cock::mkdir {

namespace {

constexpr mode_t DEFAULT_MODE = 0755;
constexpr int MADE = 0;

}

int run(int argc, char **argv) {
	if (argumentCount(argc) < ONE_ARGUMENT) return usage(argv[0], "path...");

	int status = STATUS_OK;
	for (int index = FIRST_ARGUMENT; index < argc; index++)
		if (::mkdir(argv[index], DEFAULT_MODE) != MADE) status = fail(argv[0], argv[index]);
	return status;
}

} // namespace cock::mkdir
