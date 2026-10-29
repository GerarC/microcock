#include <cock/user/tool.hpp>
#include <stdio.h>
#include <uname/command.hpp>
#include <uname/version.hpp>

namespace cock::uname {

int run(int, char **) {
	printf("microcock v%u.%u.%u\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	return cock::user::tool::STATUS_OK;
}

} // namespace cock::uname
