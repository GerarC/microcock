#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <sh/arguments.hpp>
#include <sh/commands.hpp>
#include <sh/file_commands.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::abi::fs::FileType;
using cock::abi::fs::NO_HANDLE;
using cock::abi::fs::Status;
using cock::shell::Words;

namespace cock::sh {

namespace {

constexpr int STDOUT_DESCRIPTOR = 1;
constexpr char ROOT_PATH[] = "/";
constexpr size_t FILE_CHUNK_BYTES = 512;
constexpr size_t NAME_BYTES = 256;
constexpr uint32_t START_OFFSET = 0;
constexpr size_t NO_LENGTH = 0;

cock::user::fs::Client fs;
bool fsReady = false;

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
	if (fs.lastStatus() == Status::NOT_CONNECTED) fsReady = false;
	printf("sh: %s: %s: %s\n", command, path, statusText(fs.lastStatus()));
}

} // namespace

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
	size_t length = NO_LENGTH;
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

} // namespace cock::sh
