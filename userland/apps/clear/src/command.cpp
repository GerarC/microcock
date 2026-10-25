#include <clear/command.hpp>
#include <cock/user/console.hpp>
#include <cock/user/tool.hpp>
#include <stdio.h>

namespace cock::clear {

int run(int, char **argv) {
	cock::user::console::Client console;
	if (console.connect() && console.clear()) return cock::user::tool::STATUS_OK;

	printf("%s: the console server does not answer\n", argv[0]);
	return cock::user::tool::STATUS_FAILURE;
}

} // namespace cock::clear
