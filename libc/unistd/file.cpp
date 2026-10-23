#include <cock/abi/fs.hpp>
#include <cock/user/fs.hpp>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr int FIRST_FILE_DESCRIPTOR = 3;
constexpr size_t MAX_FILES = 16;
constexpr int FAILURE = -1;
constexpr int SUCCESS = 0;
constexpr uint32_t START_OFFSET = 0;
constexpr uint32_t NO_BYTES = 0;
constexpr uint32_t MAX_OFFSET = 0x7FFFFFFF;

struct File {
	bool used;
	bool readable;
	bool writable;
	bool append;
	uint32_t handle;
	uint32_t offset;
};

cock::user::fs::Client client;
bool connected = false;
File files[MAX_FILES];

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

int failWith(int code) {
	errno = code;
	return FAILURE;
}

int failFromServer() {
	cock::abi::fs::Status status = client.lastStatus();
	if (status == cock::abi::fs::Status::NOT_CONNECTED) {
		connected = false;
		for (File &file : files)
			file.used = false;
	}
	return failWith(errorFor(status));
}

bool ensureConnected() {
	if (!connected) {
		client.onForeign = cock::user::stash_message;
		connected = client.connect();
	}
	return connected;
}

File *find(int fd) {
	if (fd < FIRST_FILE_DESCRIPTOR || fd >= FIRST_FILE_DESCRIPTOR + static_cast<int>(MAX_FILES))
		return nullptr;
	File &file = files[fd - FIRST_FILE_DESCRIPTOR];
	return file.used ? &file : nullptr;
}

}

extern "C" {

int open(const char *path, int flags, ...) {
	int access = flags & O_ACCMODE;
	if (path == nullptr || (access != O_RDONLY && access != O_WRONLY && access != O_RDWR))
		return failWith(EINVAL);

	size_t slot = 0;
	while (slot < MAX_FILES && files[slot].used)
		slot++;
	if (slot == MAX_FILES) return failWith(EMFILE);
	if (!ensureConnected()) return failWith(ENOTCONN);

	uint32_t serverFlags = cock::abi::fs::OPEN_READ_ONLY;
	if (access != O_RDONLY) serverFlags |= cock::abi::fs::OPEN_WRITE;
	if ((flags & O_CREAT) != 0) serverFlags |= cock::abi::fs::OPEN_CREATE;
	if ((flags & O_TRUNC) != 0) serverFlags |= cock::abi::fs::OPEN_TRUNCATE;

	uint32_t handle = client.open(path, serverFlags);
	if (handle == cock::abi::fs::NO_HANDLE) return failFromServer();

	files[slot] = {true, access != O_WRONLY, access != O_RDONLY, (flags & O_APPEND) != 0,
				   handle, START_OFFSET};
	return static_cast<int>(slot) + FIRST_FILE_DESCRIPTOR;
}

int close(int fd) {
	File *file = find(fd);
	if (file == nullptr) return failWith(EBADF);

	bool closed = client.close(file->handle);
	int code = closed ? SUCCESS : errorFor(client.lastStatus());
	*file = {};
	if (closed) return SUCCESS;
	return failWith(code);
}

ssize_t __file_read(int fd, void *buf, size_t count) {
	File *file = find(fd);
	if (file == nullptr || !file->readable) return failWith(EBADF);
	if (buf == nullptr) return failWith(EFAULT);
	if (count == NO_BYTES) return SUCCESS;

	long got = client.read(file->handle, file->offset, buf, count);
	if (got < 0) return failFromServer();
	file->offset += static_cast<uint32_t>(got);
	return static_cast<ssize_t>(got);
}

ssize_t __file_write(int fd, const void *buf, size_t count) {
	File *file = find(fd);
	if (file == nullptr || !file->writable) return failWith(EBADF);
	if (buf == nullptr) return failWith(EFAULT);

	if (file->append) {
		cock::user::fs::FileInfo info = {};
		if (!client.stat(file->handle, info)) return failFromServer();
		file->offset = info.size;
	}

	const char *bytes = static_cast<const char *>(buf);
	size_t done = NO_BYTES;
	while (done < count) {
		long put = client.write(file->handle, file->offset, bytes + done, count - done);
		if (put <= 0) break;
		file->offset += static_cast<uint32_t>(put);
		done += static_cast<size_t>(put);
	}
	if (done == NO_BYTES && count != NO_BYTES) return failFromServer();
	return static_cast<ssize_t>(done);
}

off_t lseek(int fd, off_t offset, int whence) {
	File *file = find(fd);
	if (file == nullptr) return failWith(EBADF);

	int64_t base = 0;
	if (whence == SEEK_CUR) {
		base = file->offset;
	} else if (whence == SEEK_END) {
		cock::user::fs::FileInfo info = {};
		if (!client.stat(file->handle, info)) return failFromServer();
		base = info.size;
	} else if (whence != SEEK_SET) {
		return failWith(EINVAL);
	}

	int64_t target = base + offset;
	if (target < static_cast<int64_t>(START_OFFSET) || target > static_cast<int64_t>(MAX_OFFSET))
		return failWith(EINVAL);
	file->offset = static_cast<uint32_t>(target);
	return static_cast<off_t>(target);
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
