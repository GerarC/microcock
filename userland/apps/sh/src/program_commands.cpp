#include <cock/abi/exec.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/pm.hpp>
#include <cock/user/exec.hpp>
#include <cock/user/fs.hpp>
#include <sh/arguments.hpp>
#include <sh/manager.hpp>
#include <sh/paths.hpp>
#include <sh/program_commands.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using cock::abi::pm::Status;
using cock::shell::Words;
using cock::user::pm::Client;
using cock::user::pm::Forked;
using cock::user::pm::Waited;

namespace cock::sh {

namespace {

constexpr char PROGRAM_DIRECTORY[] = "/bin/";
constexpr size_t PROGRAM_DIRECTORY_BYTES = sizeof(PROGRAM_DIRECTORY) - sizeof(char);
constexpr char PATH_SEPARATOR = '/';
constexpr char RUN_LABEL[] = "run";

static_assert(cock::shell::MAX_WORDS <= cock::abi::exec::MAX_ARGS, "every word of a line must fit the arguments of exec");

int lastExitStatus = STATUS_OK;

bool namesPath(const char *name) {
	for (size_t index = 0; name[index] != cock::shell::TERMINATOR; index++)
		if (name[index] == PATH_SEPARATOR) return true;
	return false;
}

bool programPath(const char *name, char (&out)[PATH_BYTES]) {
	if (namesPath(name)) return resolvePath(RUN_LABEL, name, out);

	size_t length = strlen(name);
	if (PROGRAM_DIRECTORY_BYTES + length >= sizeof(out)) {
		printf("sh: %s: name too long\n", name);
		return false;
	}
	memcpy(out, PROGRAM_DIRECTORY, PROGRAM_DIRECTORY_BYTES);
	memcpy(out + PROGRAM_DIRECTORY_BYTES, name, length + sizeof(char));
	return true;
}

[[noreturn]] void becomeProgram(Client &manager, const char *path, const Words &words) {
	const char *argv[cock::shell::MAX_WORDS] = {};
	for (size_t index = 0; index < words.count; index++)
		argv[index] = words.items[index];

	int status = STATUS_NOT_EXECUTABLE;
	cock::user::fs::Client files;
	if (files.connect(cock::abi::fs::SERVER_NAME, true)) {
		cock::user::ExecFailure failure =
			cock::user::exec(manager, files, path, argv, static_cast<uint32_t>(words.count));
		if (failure == cock::user::ExecFailure::NO_FILE) {
			printf("sh: %s: command not found\n", words.items[FIRST_WORD]);
			status = STATUS_NOT_FOUND;
		} else {
			printf("sh: %s: cannot execute\n", words.items[FIRST_WORD]);
		}
	}
	_exit(status);
}

int notStarted(const char *name) {
	printf("sh: %s: cannot start the program\n", name);
	return STATUS_NOT_STARTED;
}

int start(const Words &words) {
	const char *name = words.items[FIRST_WORD];
	char path[PATH_BYTES] = {};
	if (!programPath(name, path)) return STATUS_NOT_STARTED;

	Client *manager = processManager();
	if (manager == nullptr) return notStarted(name);

	Forked forked = manager->fork();
	if (forked.isChild()) becomeProgram(*manager, path, words);
	if (forked.status != Status::OK) {
		if (forked.status == Status::BAD_REQUEST) forgetProcessManager();
		return notStarted(name);
	}

	Waited waited = manager->wait(forked.pid);
	if (waited.status != Status::OK) {
		forgetProcessManager();
		return notStarted(name);
	}
	return static_cast<int>(waited.exitStatus);
}

} // namespace

int runProgram(const Words &words) {
	lastExitStatus = start(words);
	return lastExitStatus;
}

int lastStatus() {
	return lastExitStatus;
}

} // namespace cock::sh
