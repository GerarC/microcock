#include <sh/arguments.hpp>
#include <sh/commands.hpp>
#include <sh/exit_command.hpp>
#include <sh/program_commands.hpp>
#include <stdint.h>
#include <unistd.h>

using cock::shell::Words;

namespace cock::sh {

void exitCommand(const Words &words) {
	if (words.count > ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "[status]");

	uint32_t status = static_cast<uint32_t>(lastStatus());
	if (words.count == ONE_ARGUMENT && !cock::shell::parseNumber(words.items[SECOND_WORD], status))
		return usage(words.items[FIRST_WORD], "[status]");

	_exit(static_cast<int>(status));
}

} // namespace cock::sh
