#include <cock/user/tool.hpp>
#include <cp/command.hpp>
#include <cp/copy.hpp>

using namespace cock::user::tool;

namespace cock::cp {

namespace {

constexpr int SECOND_ARGUMENT = 2;

}

int run(int argc, char **argv) {
	if (argumentCount(argc) != TWO_ARGUMENTS) return usage(argv[0], "from to");

	return copyFile(argv[0], argv[FIRST_ARGUMENT], argv[SECOND_ARGUMENT]);
}

} // namespace cock::cp
