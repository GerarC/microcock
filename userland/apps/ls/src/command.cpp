#include <cock/user/tool.hpp>
#include <ls/command.hpp>
#include <ls/directory.hpp>

using namespace cock::user::tool;

namespace cock::ls {

namespace {

constexpr char CURRENT_PATH[] = ".";

}

int run(int argc, char **argv) {
	int count = argumentCount(argc);
	if (count > ONE_ARGUMENT) return usage(argv[0], "[path]");

	return list(argv[0], count == ONE_ARGUMENT ? argv[FIRST_ARGUMENT] : CURRENT_PATH);
}

} // namespace cock::ls
