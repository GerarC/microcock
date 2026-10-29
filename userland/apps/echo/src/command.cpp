#include <cock/user/tool.hpp>
#include <echo/command.hpp>
#include <stdio.h>

namespace cock::echo {

namespace {

constexpr char SEPARATOR = ' ';
constexpr char NEWLINE = '\n';

}

int run(int argc, char **argv) {
	for (int index = cock::user::tool::FIRST_ARGUMENT; index < argc; index++) {
		if (index > cock::user::tool::FIRST_ARGUMENT) putchar(SEPARATOR);
		printf("%s", argv[index]);
	}
	putchar(NEWLINE);
	return cock::user::tool::STATUS_OK;
}

} // namespace cock::echo
