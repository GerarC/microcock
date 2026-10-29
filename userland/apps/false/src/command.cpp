#include <cock/user/tool.hpp>
#include <false/command.hpp>

namespace cock::false_command {

int run(int, char **) {
	return cock::user::tool::STATUS_FAILURE;
}

} // namespace cock::false_command
