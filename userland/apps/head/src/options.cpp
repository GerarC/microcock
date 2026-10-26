#include <cock/user/number.hpp>
#include <cock/user/tool.hpp>
#include <head/options.hpp>
#include <string.h>

using namespace cock::user::tool;

namespace cock::head {

namespace {

constexpr char LINES_OPTION[] = "-n";
constexpr int OPTION_ARGUMENTS = 2;

}

bool parseOptions(int argc, char **argv, Options &options) {
	options.firstPath = FIRST_ARGUMENT;
	if (argumentCount(argc) == NO_ARGUMENTS || strcmp(argv[FIRST_ARGUMENT], LINES_OPTION) != 0) return true;

	if (argumentCount(argc) < OPTION_ARGUMENTS) return false;
	if (!parseUnsigned(argv[FIRST_ARGUMENT + ONE_ARGUMENT], options.lines)) return false;
	options.firstPath = FIRST_ARGUMENT + OPTION_ARGUMENTS;
	return true;
}

} // namespace cock::head
