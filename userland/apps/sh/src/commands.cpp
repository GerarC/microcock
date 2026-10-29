#include <cock/shell/pipeline.hpp>
#include <cock/shell/words.hpp>
#include <sh/arguments.hpp>
#include <sh/commands.hpp>
#include <sh/directory_commands.hpp>
#include <sh/exit_command.hpp>
#include <sh/program_commands.hpp>
#include <sh/variables.hpp>
#include <stddef.h>
#include <stdio.h>

using cock::shell::FIRST_STAGE;
using cock::shell::Pipeline;
using cock::shell::PipelineResult;
using cock::shell::Words;

namespace cock::sh {

namespace {

constexpr size_t NO_WORDS = 0;
constexpr char PROGRAM_NAMES[] = "ls cat mkdir rmdir rm mv echo ps sleep clear uname true false date";

void helpCommand(const Words &);

struct Command {
	const char *name;
	const char *summary;
	void (*run)(const Words &);
};

constexpr Command COMMANDS[] = {
	{"help", "list the commands", helpCommand},
	{"cd", "cd [path]: change the directory", changeDirectoryCommand},
	{"pwd", "print the directory", printDirectoryCommand},
	{"set", "print the variables set with NAME=value", setCommand},
	{"unset", "unset name...: remove variables", unsetCommand},
	{"exit", "exit [status]: end the shell", exitCommand},
};

constexpr size_t COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

void helpCommand(const Words &) {
	for (size_t index = 0; index < COMMAND_COUNT; index++)
		printf("  %s: %s\n", COMMANDS[index].name, COMMANDS[index].summary);
	printf("programs of /bin: %s\n", PROGRAM_NAMES);
	printf("paths are absolute or relative to the directory of cd, for example /fat or /ext\n");
	printf("cmd > file, cmd >> file, cmd < file and cmd | cmd redirect the programs\n");
}

bool streq(const char *left, const char *right) {
	size_t index = 0;
	while (left[index] != cock::shell::TERMINATOR && left[index] == right[index])
		index++;
	return left[index] == right[index];
}

const Command *findCommand(const char *name) {
	for (size_t index = 0; index < COMMAND_COUNT; index++)
		if (streq(name, COMMANDS[index].name)) return &COMMANDS[index];
	return nullptr;
}

const char *describe(PipelineResult result) {
	switch (result) {
	case PipelineResult::MISSING_COMMAND:
		return "missing command";
	case PipelineResult::MISSING_FILE:
		return "missing file name after a redirection";
	case PipelineResult::TOO_MANY_STAGES:
		return "too many commands in a pipeline";
	case PipelineResult::MISPLACED_REDIRECTION:
		return "input can only be redirected on the first command and output on the last";
	case PipelineResult::OK:
		break;
	}
	return "bad command line";
}

} // namespace

void usage(const char *command, const char *arguments) {
	printf("usage: %s %s\n", command, arguments);
}

void run(char *line) {
	char expanded[cock::shell::MAX_LINE_BYTES] = {};
	if (!expandLine(line, expanded)) return;

	Words words = {};
	cock::shell::SplitResult split = cock::shell::split(expanded, words);
	if (split == cock::shell::SplitResult::TOO_MANY_WORDS) {
		printf("sh: too many words\n");
		return;
	}
	if (split == cock::shell::SplitResult::UNTERMINATED_QUOTE) {
		printf("sh: unterminated quote\n");
		return;
	}
	if (words.count == NO_WORDS) return;
	if (assignVariables(words)) return;

	Pipeline pipeline = {};
	PipelineResult parsed = cock::shell::parsePipeline(words, pipeline);
	if (parsed != PipelineResult::OK) {
		printf("sh: %s\n", describe(parsed));
		return;
	}

	bool plain = pipeline.count == cock::shell::SINGLE_STAGE && !cock::shell::isRedirected(pipeline.stages[FIRST_STAGE]);
	for (size_t stage = 0; stage < pipeline.count; stage++) {
		const Command *command = findCommand(pipeline.stages[stage].argv[FIRST_WORD]);
		if (command == nullptr) continue;
		if (plain)
			command->run(words);
		else
			printf("sh: %s: a built-in command cannot be piped or redirected\n", command->name);
		return;
	}

	int status = runPipeline(pipeline);
	if (status != STATUS_OK && status != STATUS_NOT_FOUND && status != STATUS_NOT_EXECUTABLE &&
		status != STATUS_NOT_STARTED)
		printf("sh: %s: exit status %d\n", pipeline.stages[pipeline.count - cock::shell::SINGLE_STAGE].argv[FIRST_WORD],
			   status);
}

} // namespace cock::sh
