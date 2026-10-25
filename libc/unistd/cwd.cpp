#include "cwd.hpp"
#include "descriptor.hpp"
#include <cock/path/resolve.hpp>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>

using cock::libc::failWith;
using cock::libc::PATH_BUFFER_BYTES;

extern "C" int __file_is_directory(const char *path) __attribute__((weak));

namespace {

constexpr size_t NO_LENGTH = 0;
constexpr int NO_ERROR = 0;

char current[PATH_BUFFER_BYTES] = "/";

size_t lengthOf(const char *text) {
	size_t length = NO_LENGTH;
	while (text[length] != cock::path::TERMINATOR) length++;
	return length;
}

void copyPath(char *to, const char *from) {
	size_t index = NO_LENGTH;
	for (; from[index] != cock::path::TERMINATOR; index++) to[index] = from[index];
	to[index] = cock::path::TERMINATOR;
}

}

namespace cock::libc {

bool absolutePath(const char *path, char (&out)[PATH_BUFFER_BYTES]) {
	if (path[NO_LENGTH] == cock::path::TERMINATOR) {
		failWith(ENOENT);
		return false;
	}
	if (!cock::path::resolve(current, path, out, sizeof(out))) {
		failWith(ENAMETOOLONG);
		return false;
	}
	return true;
}

}

extern "C" {

int chdir(const char *path) {
	if (path == nullptr) return failWith(EINVAL);
	char absolute[PATH_BUFFER_BYTES];
	if (!cock::libc::absolutePath(path, absolute)) return cock::libc::FAILURE;
	if (__file_is_directory == nullptr) return failWith(ENOTSUP);

	int error = __file_is_directory(absolute);
	if (error != NO_ERROR) return failWith(error);
	copyPath(current, absolute);
	return NO_ERROR;
}

char *getcwd(char *buf, size_t size) {
	if (buf == nullptr || size == NO_LENGTH) {
		failWith(EINVAL);
		return nullptr;
	}
	if (lengthOf(current) >= size) {
		failWith(ERANGE);
		return nullptr;
	}
	copyPath(buf, current);
	return buf;
}

void __cwd_export(cock::abi::exec::ArgsBlock *block) {
	copyPath(block->cwd, current);
}

void __cwd_import(const cock::abi::exec::ArgsBlock *block) {
	if (block->cwd[NO_LENGTH] != cock::path::TERMINATOR) copyPath(current, block->cwd);
}

}
