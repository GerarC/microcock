#include <cat/command.hpp>
#include <cat/copy.hpp>
#include <cock/user/tool.hpp>

using namespace cock::user::tool;

namespace cock::cat {

int run(int argc, char **argv) {
	if (argumentCount(argc) == NO_ARGUMENTS) return copyStandardInput(argv[0]);

	int status = STATUS_OK;
	for (int index = FIRST_ARGUMENT; index < argc; index++)
		if (copyFile(argv[0], argv[index]) != STATUS_OK) status = STATUS_FAILURE;
	return status;
}

} // namespace cock::cat
