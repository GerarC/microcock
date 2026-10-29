#include <cock/user/tool.hpp>
#include <true/command.hpp>

namespace cock::true_command {

int run(int, char **) {
	return cock::user::tool::STATUS_OK;
}

} // namespace cock::true_command
