#include <cock/user/tool.hpp>
#include <mv/command.hpp>
#include <unistd.h>

using namespace cock::user::tool;

namespace cock::mv {

namespace {

constexpr int SECOND_ARGUMENT = 2;
constexpr int MOVED = 0;

}

int run(int argc, char **argv) {
	if (argumentCount(argc) != TWO_ARGUMENTS) return usage(argv[0], "from to");

	if (::rename(argv[FIRST_ARGUMENT], argv[SECOND_ARGUMENT]) != MOVED) return fail(argv[0], argv[FIRST_ARGUMENT]);
	return STATUS_OK;
}

} // namespace cock::mv
