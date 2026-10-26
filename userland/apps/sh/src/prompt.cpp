#include <sh/paths.hpp>
#include <sh/prompt.hpp>
#include <sh/version.hpp>
#include <stdio.h>
#include <unistd.h>

namespace cock::sh {

namespace {

constexpr char GREEN[] = "\x1B[92m";
constexpr char CYAN[] = "\x1B[96m";
constexpr char YELLOW[] = "\x1B[93m";
constexpr char RESET[] = "\x1B[0m";
constexpr char UNKNOWN_DIRECTORY[] = "?";

} // namespace

void printPrompt() {
	char directory[PATH_BYTES] = {};
	const char *shown = getcwd(directory, sizeof(directory)) != nullptr ? directory : UNKNOWN_DIRECTORY;
	printf("%scock%s:%s%s%s> ", GREEN, RESET, CYAN, shown, RESET);
}

void printBanner() {
	printf("%smicrocock%s v%u.%u.%u shell, type %shelp%s\n", GREEN, RESET, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
		   YELLOW, RESET);
}

} // namespace cock::sh
