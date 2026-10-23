#include <cock/shell/words.hpp>
#include <sh/commands.hpp>
#include <sh/input.hpp>
#include <sh/version.hpp>
#include <stdio.h>

namespace {

constexpr char PROMPT[] = "cock> ";

} // namespace

extern "C" int main() {
	printf("microcock v%u.%u.%u shell, type help\n", cock::sh::VERSION_MAJOR,
		   cock::sh::VERSION_MINOR, cock::sh::VERSION_PATCH);

	char line[cock::shell::MAX_LINE_BYTES] = {};
	for (;;) {
		printf(PROMPT);
		cock::sh::readLine(line);
		cock::sh::run(line);
	}
}
