#include <cock/shell/words.hpp>
#include <cock/user/console.hpp>
#include <sh/commands.hpp>
#include <sh/input.hpp>
#include <sh/prompt.hpp>

extern "C" int main() {
	__console_attach();
	cock::sh::printBanner();

	char line[cock::shell::MAX_LINE_BYTES] = {};
	for (;;) {
		cock::sh::printPrompt();
		cock::sh::readLine(line);
		cock::sh::run(line);
	}
}
