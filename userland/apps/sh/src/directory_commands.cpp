#include <errno.h>
#include <sh/arguments.hpp>
#include <sh/commands.hpp>
#include <sh/directory_commands.hpp>
#include <sh/paths.hpp>
#include <stdio.h>
#include <unistd.h>

using cock::shell::Words;

namespace cock::sh {

namespace {

constexpr char ROOT_PATH[] = "/";

const char *reasonFor(int code) {
	switch (code) {
	case ENOENT:
		return "not found";
	case ENOTDIR:
		return "not a directory";
	case ENAMETOOLONG:
		return "path too long";
	default:
		return "failed";
	}
}

} // namespace

void changeDirectoryCommand(const Words &words) {
	if (words.count > ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "[path]");

	const char *path = words.count == ONE_ARGUMENT ? words.items[SECOND_WORD] : ROOT_PATH;
	if (chdir(path) != 0) printf("sh: cd: %s: %s\n", path, reasonFor(errno));
}

void printDirectoryCommand(const Words &) {
	char directory[PATH_BYTES] = {};
	if (getcwd(directory, sizeof(directory)) == nullptr) {
		printf("sh: pwd: failed\n");
		return;
	}
	printf("%s\n", directory);
}

} // namespace cock::sh
