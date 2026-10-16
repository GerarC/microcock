#include <cock/abi/fs.hpp>
#include <cock/shell/words.hpp>
#include <cock/user/api.hpp>
#include <cock/user/console.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::abi::fs::FileType;
using cock::abi::fs::NO_HANDLE;
using cock::abi::fs::Status;
using cock::shell::Words;

#ifdef SH_VERSION_MAJOR
constexpr unsigned VERSION_MAJOR = SH_VERSION_MAJOR;
constexpr unsigned VERSION_MINOR = SH_VERSION_MINOR;
constexpr unsigned VERSION_PATCH = SH_VERSION_PATCH;
#else
constexpr unsigned VERSION_MAJOR = 0;
constexpr unsigned VERSION_MINOR = 0;
constexpr unsigned VERSION_PATCH = 0;
#endif

namespace {

constexpr int STDIN_DESCRIPTOR = 0;
constexpr int STDOUT_DESCRIPTOR = 1;
constexpr char PROMPT[] = "cock> ";
constexpr char NEWLINE = '\n';
constexpr char ROOT_PATH[] = "/";
constexpr size_t READ_CHUNK_BYTES = 12;
constexpr size_t FILE_CHUNK_BYTES = 512;
constexpr size_t NAME_BYTES = 256;
constexpr size_t TEXT_BYTES = 256;
constexpr uint32_t START_OFFSET = 0;
constexpr uint32_t MILLISECONDS_PER_SECOND = 1000;
constexpr uint32_t MAX_SLEEP_SECONDS = 3600;
constexpr uint32_t RETRY_MS = 100;
constexpr size_t SECOND_WORD = 1;
constexpr size_t THIRD_WORD = 2;
constexpr size_t FOURTH_WORD = 3;
constexpr size_t NO_ARGUMENTS = 1;
constexpr size_t ONE_ARGUMENT = 2;
constexpr size_t TWO_ARGUMENTS = 3;
constexpr size_t FIRST_WORD = 0;

cock::user::fs::Client fs;
cock::user::console::Client console;
bool fsReady = false;
bool consoleReady = false;

const char *statusText(Status status) {
	switch (status) {
	case Status::OK:
		return "ok";
	case Status::NOT_CONNECTED:
		return "not connected";
	case Status::BAD_REQUEST:
		return "bad request";
	case Status::NOT_FOUND:
		return "not found";
	case Status::BAD_HANDLE:
		return "bad handle";
	case Status::OUT_OF_RANGE:
		return "out of range";
	case Status::NO_SPACE:
		return "no space";
	case Status::UNSUPPORTED:
		return "unsupported";
	case Status::IO_ERROR:
		return "i/o error";
	case Status::READ_ONLY:
		return "read only";
	case Status::BUSY:
		return "busy";
	case Status::EXISTS:
		return "already exists";
	case Status::NOT_EMPTY:
		return "not empty";
	case Status::CROSS_DEVICE:
		return "different volumes";
	}
	return "error";
}

bool connectFiles() {
	if (fsReady) return true;
	fs.onForeign = cock::user::stash_message;
	fsReady = fs.connect();
	if (!fsReady) printf("sh: cannot reach the file server\n");
	return fsReady;
}

void failFile(const char *command, const char *path) {
	printf("sh: %s: %s: %s\n", command, path, statusText(fs.lastStatus()));
}

void usage(const char *command, const char *arguments) {
	printf("usage: %s %s\n", command, arguments);
}

void helpCommand(const Words &);

void versionCommand(const Words &) {
	printf("microcock v%u.%u.%u\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
}

void echoCommand(const Words &words) {
	char text[TEXT_BYTES] = {};
	size_t length = 0;
	if (!cock::shell::join(words, SECOND_WORD, text, sizeof(text), length)) {
		printf("sh: echo: text too long\n");
		return;
	}
	printf("%s\n", text);
}

void listCommand(const Words &words) {
	if (words.count > ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "[path]");
	if (!connectFiles()) return;

	const char *path = words.count == ONE_ARGUMENT ? words.items[SECOND_WORD] : ROOT_PATH;
	uint32_t handle = fs.open(path);
	if (handle == NO_HANDLE) return failFile("ls", path);

	cock::user::fs::FileInfo info = {};
	if (!fs.stat(handle, info)) {
		failFile("ls", path);
		fs.close(handle);
		return;
	}
	if (info.type != FileType::DIRECTORY) {
		printf("%s\n", path);
		fs.close(handle);
		return;
	}

	char name[NAME_BYTES] = {};
	FileType type = FileType::REGULAR;
	for (uint32_t index = 0; fs.readdir(handle, index, name, sizeof(name), type); index++)
		printf("%s%s\n", name, type == FileType::DIRECTORY ? "/" : "");
	fs.close(handle);
}

void catCommand(const Words &words) {
	if (words.count != ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "path");
	if (!connectFiles()) return;

	const char *path = words.items[SECOND_WORD];
	uint32_t handle = fs.open(path);
	if (handle == NO_HANDLE) return failFile("cat", path);

	cock::user::fs::FileInfo info = {};
	if (!fs.stat(handle, info)) {
		failFile("cat", path);
		fs.close(handle);
		return;
	}
	if (info.type == FileType::DIRECTORY) {
		printf("sh: cat: %s: is a directory\n", path);
		fs.close(handle);
		return;
	}

	char chunk[FILE_CHUNK_BYTES] = {};
	uint32_t offset = START_OFFSET;
	for (;;) {
		long count = fs.read(handle, offset, chunk, sizeof(chunk));
		if (count < 0) {
			failFile("cat", path);
			break;
		}
		if (count == 0) break;
		write(STDOUT_DESCRIPTOR, chunk, static_cast<size_t>(count));
		offset += static_cast<uint32_t>(count);
	}
	fs.close(handle);
}

void writeCommand(const Words &words) {
	if (words.count < TWO_ARGUMENTS) return usage(words.items[FIRST_WORD], "path text...");
	if (!connectFiles()) return;

	const char *path = words.items[SECOND_WORD];
	char text[TEXT_BYTES] = {};
	size_t length = 0;
	if (!cock::shell::join(words, THIRD_WORD, text, sizeof(text) - sizeof(char), length)) {
		printf("sh: write: text too long\n");
		return;
	}
	text[length++] = NEWLINE;

	uint32_t flags = cock::abi::fs::OPEN_WRITE | cock::abi::fs::OPEN_CREATE |
					 cock::abi::fs::OPEN_TRUNCATE;
	uint32_t handle = fs.open(path, flags);
	if (handle == NO_HANDLE) return failFile("write", path);

	if (fs.write(handle, START_OFFSET, text, length) != static_cast<long>(length))
		failFile("write", path);
	fs.close(handle);
}

void makeDirectoryCommand(const Words &words) {
	if (words.count != ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "path");
	if (!connectFiles()) return;
	if (!fs.mkdir(words.items[SECOND_WORD])) failFile("mkdir", words.items[SECOND_WORD]);
}

void removeDirectoryCommand(const Words &words) {
	if (words.count != ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "path");
	if (!connectFiles()) return;
	if (!fs.rmdir(words.items[SECOND_WORD])) failFile("rmdir", words.items[SECOND_WORD]);
}

void removeCommand(const Words &words) {
	if (words.count != ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "path");
	if (!connectFiles()) return;
	if (!fs.unlink(words.items[SECOND_WORD])) failFile("rm", words.items[SECOND_WORD]);
}

void moveCommand(const Words &words) {
	if (words.count != TWO_ARGUMENTS) return usage(words.items[FIRST_WORD], "from to");
	if (!connectFiles()) return;
	if (!fs.rename(words.items[SECOND_WORD], words.items[THIRD_WORD]))
		failFile("mv", words.items[SECOND_WORD]);
}

void sleepCommand(const Words &words) {
	uint32_t seconds = 0;
	if (words.count != ONE_ARGUMENT ||
		!cock::shell::parseNumber(words.items[SECOND_WORD], seconds) ||
		seconds > MAX_SLEEP_SECONDS)
		return usage(words.items[FIRST_WORD], "seconds");

	cock::user::sleep_ms(seconds * MILLISECONDS_PER_SECOND);
}

void clearCommand(const Words &) {
	if (!consoleReady) consoleReady = console.connect();
	if (!consoleReady || !console.clear()) printf("sh: clear: the console server does not answer\n");
}

struct Command {
	const char *name;
	const char *summary;
	void (*run)(const Words &);
};

constexpr Command COMMANDS[] = {
	{"help", "list the commands", helpCommand},
	{"version", "print the version", versionCommand},
	{"echo", "echo text...: print the text", echoCommand},
	{"ls", "ls [path]: list a directory", listCommand},
	{"cat", "cat path: print a file", catCommand},
	{"write", "write path text...: replace a file with the text", writeCommand},
	{"mkdir", "mkdir path: make a directory", makeDirectoryCommand},
	{"rmdir", "rmdir path: remove an empty directory", removeDirectoryCommand},
	{"rm", "rm path: delete a file", removeCommand},
	{"mv", "mv from to: rename or move", moveCommand},
	{"sleep", "sleep seconds: wait", sleepCommand},
	{"clear", "clear the screen", clearCommand},
};

constexpr size_t COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

void helpCommand(const Words &) {
	for (size_t index = 0; index < COMMAND_COUNT; index++)
		printf("  %s: %s\n", COMMANDS[index].name, COMMANDS[index].summary);
	printf("paths are absolute, for example /fat or /ext\n");
}

bool streq(const char *left, const char *right) {
	size_t index = 0;
	while (left[index] != cock::shell::TERMINATOR && left[index] == right[index])
		index++;
	return left[index] == right[index];
}

void run(char *line) {
	Words words = {};
	if (!cock::shell::split(line, words)) {
		printf("sh: too many words\n");
		return;
	}
	if (words.count == 0) return;

	for (size_t index = 0; index < COMMAND_COUNT; index++) {
		if (streq(words.items[FIRST_WORD], COMMANDS[index].name)) {
			COMMANDS[index].run(words);
			return;
		}
	}
	printf("sh: %s: command not found\n", words.items[FIRST_WORD]);
}

void readLine(char *line) {
	size_t length = 0;
	bool complete = false;

	while (!complete) {
		char chunk[READ_CHUNK_BYTES] = {};
		ssize_t count = read(STDIN_DESCRIPTOR, chunk, sizeof(chunk));
		if (count <= 0) {
			cock::user::sleep_ms(RETRY_MS);
			continue;
		}

		for (ssize_t index = 0; index < count && !complete; index++) {
			if (chunk[index] == NEWLINE) {
				complete = true;
			} else if (length + 1 < cock::shell::MAX_LINE_BYTES) {
				line[length++] = chunk[index];
			}
		}
	}
	line[length] = cock::shell::TERMINATOR;
}

} // namespace

extern "C" void _start() {
	printf("microcock v%u.%u.%u shell, type help\n", VERSION_MAJOR, VERSION_MINOR,
		   VERSION_PATCH);

	char line[cock::shell::MAX_LINE_BYTES] = {};
	for (;;) {
		printf(PROMPT);
		readLine(line);
		run(line);
	}
}
