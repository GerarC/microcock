#include <cock/abi/exec.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/pm.hpp>
#include <cock/user/exec.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/tool.hpp>
#include <fcntl.h>
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
using cock::shell::MAX_STAGES;
using cock::shell::Pipeline;
using cock::shell::Stage;
using cock::user::pm::Client;
using cock::user::pm::Forked;
using cock::user::pm::Waited;

namespace cock::sh {

namespace {

constexpr char PROGRAM_DIRECTORY[] = "/bin/";
constexpr size_t PROGRAM_DIRECTORY_BYTES = sizeof(PROGRAM_DIRECTORY) - sizeof(char);
constexpr char PATH_SEPARATOR = '/';
constexpr char RUN_LABEL[] = "run";
constexpr char SHELL_NAME[] = "sh";
constexpr char PIPE_SUBJECT[] = "pipe";
constexpr int NO_DESCRIPTOR = -1;
constexpr int PIPE_READ_END = 0;
constexpr int PIPE_WRITE_END = 1;
constexpr int PIPE_END_COUNT = 2;
constexpr int PIPE_CREATED = 0;
constexpr int STANDARD_INPUT = 0;
constexpr int STANDARD_OUTPUT = 1;
constexpr int INPUT_FLAGS = O_RDONLY;
constexpr int OUTPUT_FLAGS = O_WRONLY | O_CREAT;

static_assert(cock::shell::MAX_WORDS <= cock::abi::exec::MAX_ARGS, "every word of a line must fit the arguments of exec");

int lastExitStatus = STATUS_OK;

struct Wiring {
	int input;
	int output;
	int unused;
};

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

void closeDescriptor(int descriptor) {
	if (descriptor != NO_DESCRIPTOR) close(descriptor);
}

bool plug(int from, int target) {
	if (from == target) return true;
	bool done = dup2(from, target) == target;
	close(from);
	return done;
}

bool redirect(int target, const char *path, int flags) {
	int descriptor = open(path, flags);
	if (descriptor < 0) {
		cock::user::tool::fail(SHELL_NAME, path);
		return false;
	}
	return plug(descriptor, target);
}

bool wire(const Stage &stage, const Wiring &wiring) {
	bool ready = true;
	if (wiring.input != NO_DESCRIPTOR) ready = plug(wiring.input, STANDARD_INPUT);
	if (ready && wiring.output != NO_DESCRIPTOR) ready = plug(wiring.output, STANDARD_OUTPUT);
	closeDescriptor(wiring.unused);
	if (ready && stage.input != nullptr) ready = redirect(STANDARD_INPUT, stage.input, INPUT_FLAGS);
	if (ready && stage.output != nullptr)
		ready = redirect(STANDARD_OUTPUT, stage.output, OUTPUT_FLAGS | (stage.append ? O_APPEND : O_TRUNC));
	return ready;
}

[[noreturn]] void becomeProgram(Client &manager, const char *path, const Stage &stage, const Wiring &wiring) {
	if (!wire(stage, wiring)) _exit(cock::user::tool::STATUS_FAILURE);

	int status = STATUS_NOT_EXECUTABLE;
	cock::user::fs::Client files;
	if (files.connect(cock::abi::fs::SERVER_NAME, true)) {
		cock::user::ExecFailure failure =
			cock::user::exec(manager, files, path, stage.argv, static_cast<uint32_t>(stage.argc));
		if (failure == cock::user::ExecFailure::NO_FILE) {
			printf("sh: %s: command not found\n", stage.argv[FIRST_WORD]);
			status = STATUS_NOT_FOUND;
		} else {
			printf("sh: %s: cannot execute\n", stage.argv[FIRST_WORD]);
		}
	}
	_exit(status);
}

int notStarted(const char *name) {
	printf("sh: %s: cannot start the program\n", name);
	return STATUS_NOT_STARTED;
}

int start(const Pipeline &pipeline) {
	char paths[MAX_STAGES][PATH_BYTES] = {};
	for (size_t index = 0; index < pipeline.count; index++)
		if (!programPath(pipeline.stages[index].argv[FIRST_WORD], paths[index])) return STATUS_NOT_STARTED;

	Client *manager = processManager();
	if (manager == nullptr) return notStarted(pipeline.stages[cock::shell::FIRST_STAGE].argv[FIRST_WORD]);

	uint32_t pids[MAX_STAGES] = {};
	size_t started = 0;
	int input = NO_DESCRIPTOR;
	bool failed = false;
	for (size_t index = 0; index < pipeline.count; index++) {
		const Stage &stage = pipeline.stages[index];
		bool last = index + cock::shell::SINGLE_STAGE == pipeline.count;
		int ends[PIPE_END_COUNT] = {NO_DESCRIPTOR, NO_DESCRIPTOR};
		if (!last && pipe(ends) != PIPE_CREATED) {
			cock::user::tool::fail(SHELL_NAME, PIPE_SUBJECT);
			failed = true;
			break;
		}

		Wiring wiring = {input, ends[PIPE_WRITE_END], ends[PIPE_READ_END]};
		Forked forked = manager->fork();
		if (forked.isChild()) becomeProgram(*manager, paths[index], stage, wiring);

		closeDescriptor(input);
		closeDescriptor(ends[PIPE_WRITE_END]);
		input = ends[PIPE_READ_END];
		if (forked.status != Status::OK) {
			if (forked.status == Status::BAD_REQUEST) forgetProcessManager();
			notStarted(stage.argv[FIRST_WORD]);
			failed = true;
			break;
		}
		pids[started++] = forked.pid;
	}
	closeDescriptor(input);

	int status = STATUS_NOT_STARTED;
	for (size_t index = 0; index < started; index++) {
		Waited waited = manager->wait(pids[index]);
		if (waited.status != Status::OK) {
			forgetProcessManager();
			failed = true;
			break;
		}
		status = static_cast<int>(waited.exitStatus);
	}
	return failed ? STATUS_NOT_STARTED : status;
}

} // namespace

int runPipeline(const Pipeline &pipeline) {
	lastExitStatus = start(pipeline);
	return lastExitStatus;
}

int lastStatus() {
	return lastExitStatus;
}

} // namespace cock::sh
