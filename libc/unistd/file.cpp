#include "descriptor.hpp"
#include <cock/abi/fs.hpp>
#include <cock/user/fs.hpp>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

using cock::libc::CLOSED;
using cock::libc::Description;
using cock::libc::dropDescriptions;
using cock::libc::failWith;
using cock::libc::FAILURE;
using cock::libc::openDescription;
using cock::libc::Operations;

namespace {

constexpr int SUCCESS = 0;
constexpr uint32_t NO_BYTES = 0;
constexpr uint32_t MAX_OFFSET = 0x7FFFFFFF;
constexpr int64_t MIN_OFFSET = 0;

cock::user::fs::Client client;
bool connected = false;
bool keepFilesOnConnect = false;
uint32_t owner = cock::user::NO_PROCESS;

int errorFor(cock::abi::fs::Status status) {
	using cock::abi::fs::Status;
	switch (status) {
	case Status::OK:
		return SUCCESS;
	case Status::NOT_CONNECTED:
		return ENOTCONN;
	case Status::BAD_REQUEST:
	case Status::OUT_OF_RANGE:
		return EINVAL;
	case Status::NOT_FOUND:
		return ENOENT;
	case Status::BAD_HANDLE:
		return EBADF;
	case Status::NO_SPACE:
		return ENOSPC;
	case Status::UNSUPPORTED:
		return ENOTSUP;
	case Status::IO_ERROR:
		return EIO;
	case Status::READ_ONLY:
		return EROFS;
	case Status::BUSY:
		return EBUSY;
	case Status::EXISTS:
		return EEXIST;
	case Status::NOT_EMPTY:
		return ENOTEMPTY;
	case Status::CROSS_DEVICE:
		return EXDEV;
	}
	return EIO;
}

const Operations &fileOperations();

int failFromServer() {
	cock::abi::fs::Status status = client.lastStatus();
	if (status == cock::abi::fs::Status::NOT_CONNECTED) {
		connected = false;
		dropDescriptions(fileOperations());
	}
	return failWith(errorFor(status));
}

bool ensureConnected() {
	uint32_t self = cock::user::self();
	bool copied = connected && owner != self;
	if (connected && !copied) return true;

	client.onForeign = cock::user::stash_message;
	connected = client.connect(cock::abi::fs::SERVER_NAME, keepFilesOnConnect);
	keepFilesOnConnect = false;
	owner = self;
	if (!connected && copied) dropDescriptions(fileOperations());
	return connected;
}

ssize_t fileRead(Description &file, void *buf, size_t count) {
	if (!ensureConnected()) return failWith(ENOTCONN);
	long got = client.read(file.handle, file.offset, buf, count);
	if (got < 0) return failFromServer();
	file.offset += static_cast<uint32_t>(got);
	return static_cast<ssize_t>(got);
}

ssize_t fileWrite(Description &file, const void *buf, size_t count) {
	if (!ensureConnected()) return failWith(ENOTCONN);
	if (file.append) {
		cock::user::fs::FileInfo info = {};
		if (!client.stat(file.handle, info)) return failFromServer();
		file.offset = info.size;
	}

	const char *bytes = static_cast<const char *>(buf);
	size_t done = NO_BYTES;
	while (done < count) {
		long put = client.write(file.handle, file.offset, bytes + done, count - done);
		if (put <= 0) break;
		file.offset += static_cast<uint32_t>(put);
		done += static_cast<size_t>(put);
	}
	if (done == NO_BYTES && count != NO_BYTES) return failFromServer();
	return static_cast<ssize_t>(done);
}

off_t fileSeek(Description &file, off_t offset, int whence) {
	int64_t base = 0;
	if (whence == SEEK_CUR) {
		base = file.offset;
	} else if (whence == SEEK_END) {
		if (!ensureConnected()) return failWith(ENOTCONN);
		cock::user::fs::FileInfo info = {};
		if (!client.stat(file.handle, info)) return failFromServer();
		base = info.size;
	} else if (whence != SEEK_SET) {
		return failWith(EINVAL);
	}

	int64_t target = base + offset;
	if (target < MIN_OFFSET || target > static_cast<int64_t>(MAX_OFFSET)) return failWith(EINVAL);
	file.offset = static_cast<uint32_t>(target);
	return static_cast<off_t>(target);
}

int fileClose(Description &file) {
	if (!ensureConnected()) return ENOTCONN;
	return client.close(file.handle) ? CLOSED : errorFor(client.lastStatus());
}

constexpr Operations FILE_OPERATIONS = {cock::abi::exec::KIND_FILE, fileRead, fileWrite, fileSeek, fileClose};

const Operations &fileOperations() {
	return FILE_OPERATIONS;
}

}

extern "C" {

const Operations *__file_operations() {
	return &FILE_OPERATIONS;
}

void __file_inherited() {
	keepFilesOnConnect = true;
}

int open(const char *path, int flags, ...) {
	int access = flags & O_ACCMODE;
	if (path == nullptr || (access != O_RDONLY && access != O_WRONLY && access != O_RDWR))
		return failWith(EINVAL);
	if (!ensureConnected()) return failWith(ENOTCONN);

	uint32_t serverFlags = cock::abi::fs::OPEN_READ_ONLY;
	if (access != O_RDONLY) serverFlags |= cock::abi::fs::OPEN_WRITE;
	if ((flags & O_CREAT) != 0) serverFlags |= cock::abi::fs::OPEN_CREATE;
	if ((flags & O_TRUNC) != 0) serverFlags |= cock::abi::fs::OPEN_TRUNCATE;

	uint32_t handle = client.open(path, serverFlags);
	if (handle == cock::abi::fs::NO_HANDLE) return failFromServer();

	int descriptor = openDescription(fileOperations(), handle, access != O_WRONLY, access != O_RDONLY,
									 (flags & O_APPEND) != 0);
	if (descriptor == FAILURE) client.close(handle);
	return descriptor;
}

int unlink(const char *path) {
	if (path == nullptr) return failWith(EINVAL);
	if (!ensureConnected()) return failWith(ENOTCONN);
	return client.unlink(path) ? SUCCESS : failFromServer();
}

int mkdir(const char *path, mode_t) {
	if (path == nullptr) return failWith(EINVAL);
	if (!ensureConnected()) return failWith(ENOTCONN);
	return client.mkdir(path) ? SUCCESS : failFromServer();
}

int rmdir(const char *path) {
	if (path == nullptr) return failWith(EINVAL);
	if (!ensureConnected()) return failWith(ENOTCONN);
	return client.rmdir(path) ? SUCCESS : failFromServer();
}

int rename(const char *from, const char *to) {
	if (from == nullptr || to == nullptr) return failWith(EINVAL);
	if (!ensureConnected()) return failWith(ENOTCONN);
	return client.rename(from, to) ? SUCCESS : failFromServer();
}

int link(const char *existing, const char *name) {
	if (existing == nullptr || name == nullptr) return failWith(EINVAL);
	if (!ensureConnected()) return failWith(ENOTCONN);
	return client.link(existing, name) ? SUCCESS : failFromServer();
}

}
