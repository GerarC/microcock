#include <cock/user/tool.hpp>
#include <fcntl.h>
#include <touch/command.hpp>
#include <unistd.h>

using namespace cock::user::tool;

namespace cock::touch {

int run(int argc, char **argv) {
	if (argumentCount(argc) == NO_ARGUMENTS) return usage(argv[0], "path...");

	int status = STATUS_OK;
	for (int index = FIRST_ARGUMENT; index < argc; index++) {
		int descriptor = ::open(argv[index], O_WRONLY | O_CREAT);
		if (descriptor < 0) {
			status = fail(argv[0], argv[index]);
			continue;
		}
		::close(descriptor);
	}
	return status;
}

} // namespace cock::touch
