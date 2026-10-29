#include <cock/user/tool.hpp>
#include <rmdir/command.hpp>
#include <unistd.h>

using namespace cock::user::tool;

namespace cock::rmdir {

namespace {

constexpr int REMOVED = 0;

}

int run(int argc, char **argv) {
	if (argumentCount(argc) < ONE_ARGUMENT) return usage(argv[0], "path...");

	int status = STATUS_OK;
	for (int index = FIRST_ARGUMENT; index < argc; index++)
		if (::rmdir(argv[index]) != REMOVED) status = fail(argv[0], argv[index]);
	return status;
}

} // namespace cock::rmdir
