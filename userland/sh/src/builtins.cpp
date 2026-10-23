#include <cock/user/api.hpp>
#include <cock/user/console.hpp>
#include <sh/arguments.hpp>
#include <sh/builtins.hpp>
#include <sh/commands.hpp>
#include <sh/version.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::shell::Words;

namespace cock::sh {

namespace {

constexpr uint32_t MILLISECONDS_PER_SECOND = 1000;
constexpr uint32_t MAX_SLEEP_SECONDS = 3600;
constexpr size_t NO_LENGTH = 0;
constexpr uint32_t NO_SECONDS = 0;

cock::user::console::Client console;
bool consoleReady = false;

} // namespace

void versionCommand(const Words &) {
	printf("microcock v%u.%u.%u\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
}

void echoCommand(const Words &words) {
	char text[TEXT_BYTES] = {};
	size_t length = NO_LENGTH;
	if (!cock::shell::join(words, SECOND_WORD, text, sizeof(text), length)) {
		printf("sh: echo: text too long\n");
		return;
	}
	printf("%s\n", text);
}

void sleepCommand(const Words &words) {
	uint32_t seconds = NO_SECONDS;
	if (words.count != ONE_ARGUMENT ||
		!cock::shell::parseNumber(words.items[SECOND_WORD], seconds) ||
		seconds > MAX_SLEEP_SECONDS)
		return usage(words.items[FIRST_WORD], "seconds");

	cock::user::sleep_ms(seconds * MILLISECONDS_PER_SECOND);
}

void clearCommand(const Words &) {
	if (!consoleReady) consoleReady = console.connect();
	if (!consoleReady || !console.clear())
		printf("sh: clear: the console server does not answer\n");
}

} // namespace cock::sh
