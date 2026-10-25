#include <cock/shell/words.hpp>
#include <sh/arguments.hpp>
#include <sh/builtins.hpp>
#include <sh/commands.hpp>
#include <sh/directory_commands.hpp>
#include <sh/file_commands.hpp>
#include <sh/process_commands.hpp>
#include <stddef.h>
#include <stdio.h>

using cock::shell::Words;

namespace cock::sh {

namespace {

constexpr size_t NO_WORDS = 0;

void helpCommand(const Words &);

struct Command {
	const char *name;
	const char *summary;
	void (*run)(const Words &);
};

constexpr Command COMMANDS[] = {
	{"help", "list the commands", helpCommand},
	{"version", "print the version", versionCommand},
	{"echo", "echo text...: print the text", echoCommand},
	{"cd", "cd [path]: change the directory", changeDirectoryCommand},
	{"pwd", "print the directory", printDirectoryCommand},
	{"ls", "ls [path]: list a directory", listCommand},
	{"cat", "cat path: print a file", catCommand},
	{"write", "write path text...: replace a file with the text", writeCommand},
	{"mkdir", "mkdir path: make a directory", makeDirectoryCommand},
	{"rmdir", "rmdir path: remove an empty directory", removeDirectoryCommand},
	{"rm", "rm path: delete a file", removeCommand},
	{"mv", "mv from to: rename or move", moveCommand},
	{"ps", "list the processes of pm", processListCommand},
	{"sleep", "sleep seconds: wait", sleepCommand},
	{"clear", "clear the screen", clearCommand},
};

constexpr size_t COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

void helpCommand(const Words &) {
	for (size_t index = 0; index < COMMAND_COUNT; index++)
		printf("  %s: %s\n", COMMANDS[index].name, COMMANDS[index].summary);
	printf("paths are absolute or relative to the directory of cd, for example /fat or /ext\n");
}

bool streq(const char *left, const char *right) {
	size_t index = 0;
	while (left[index] != cock::shell::TERMINATOR && left[index] == right[index])
		index++;
	return left[index] == right[index];
}

} // namespace

void usage(const char *command, const char *arguments) {
	printf("usage: %s %s\n", command, arguments);
}

void run(char *line) {
	Words words = {};
	if (!cock::shell::split(line, words)) {
		printf("sh: too many words\n");
		return;
	}
	if (words.count == NO_WORDS) return;

	for (size_t index = 0; index < COMMAND_COUNT; index++) {
		if (streq(words.items[FIRST_WORD], COMMANDS[index].name)) {
			COMMANDS[index].run(words);
			return;
		}
	}
	printf("sh: %s: command not found\n", words.items[FIRST_WORD]);
}

} // namespace cock::sh
