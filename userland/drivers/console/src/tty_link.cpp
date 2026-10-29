#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <console/tty_link.hpp>

namespace cock::console {

namespace {

uint32_t tty = cock::user::NO_PROCESS;

} // namespace

bool isTty(uint32_t sender) {
	if (sender != tty) tty = cock::user::names::server(cock::abi::console::TTY_NAME);
	return sender == tty && tty != cock::user::NO_PROCESS;
}

} // namespace cock::console
