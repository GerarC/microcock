#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/tool.hpp>
#include <errno.h>
#include <ls/directory.hpp>
#include <ls/location.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::fs::FileType;
using cock::abi::fs::NO_HANDLE;
using cock::abi::fs::Status;
using namespace cock::user::tool;

namespace cock::ls {

namespace {

constexpr size_t NAME_BYTES = 256;
constexpr uint32_t FIRST_ENTRY = 0;

int failStatus(const char *program, const char *path, Status status) {
	if (status == Status::NOT_CONNECTED) errno = ENOTCONN;
	else if (status == Status::NOT_FOUND) errno = ENOENT;
	else errno = EIO;
	return fail(program, path);
}

} // namespace

int list(const char *program, const char *path) {
	char absolute[PATH_BYTES] = {};
	if (!resolveLocation(path, absolute)) return fail(program, path);

	cock::user::fs::Client fs;
	fs.onForeign = cock::user::stash_message;
	if (!fs.connect()) {
		errno = ENOTCONN;
		return fail(program, path);
	}

	uint32_t handle = fs.open(absolute);
	if (handle == NO_HANDLE) return failStatus(program, path, fs.lastStatus());

	cock::user::fs::FileInfo info = {};
	if (!fs.stat(handle, info)) {
		Status status = fs.lastStatus();
		fs.close(handle);
		return failStatus(program, path, status);
	}
	if (info.type != FileType::DIRECTORY) {
		printf("%s\n", path);
		fs.close(handle);
		return STATUS_OK;
	}

	char name[NAME_BYTES] = {};
	FileType type = FileType::REGULAR;
	for (uint32_t index = FIRST_ENTRY; fs.readdir(handle, index, name, sizeof(name), type); index++)
		printf("%s%s\n", name, type == FileType::DIRECTORY ? "/" : "");
	fs.close(handle);
	return STATUS_OK;
}

} // namespace cock::ls
