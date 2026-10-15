#include <cock/abi/fs.hpp>
#include <cock/user/fs.hpp>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
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

bool ensureConnected() {
	if (!connected) connected = client.connect();
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
		return FAILURE;

	size_t slot = 0;
	while (slot < MAX_FILES && files[slot].used)
		slot++;
	if (slot == MAX_FILES || !ensureConnected()) return FAILURE;

	uint32_t serverFlags = cock::abi::fs::OPEN_READ_ONLY;
	if (access != O_RDONLY) serverFlags |= cock::abi::fs::OPEN_WRITE;
	if ((flags & O_CREAT) != 0) serverFlags |= cock::abi::fs::OPEN_CREATE;
	if ((flags & O_TRUNC) != 0) serverFlags |= cock::abi::fs::OPEN_TRUNCATE;

	uint32_t handle = client.open(path, serverFlags);
	if (handle == cock::abi::fs::NO_HANDLE) return FAILURE;

	files[slot] = {true, access != O_WRONLY, access != O_RDONLY, (flags & O_APPEND) != 0,
				   handle, START_OFFSET};
	return static_cast<int>(slot) + FIRST_FILE_DESCRIPTOR;
}

int close(int fd) {
	File *file = find(fd);
	if (file == nullptr) return FAILURE;

	bool closed = client.close(file->handle);
	*file = {};
	return closed ? SUCCESS : FAILURE;
}

ssize_t __file_read(int fd, void *buf, size_t count) {
	File *file = find(fd);
	if (file == nullptr || !file->readable || buf == nullptr) return FAILURE;
	if (count == NO_BYTES) return SUCCESS;

	long got = client.read(file->handle, file->offset, buf, count);
	if (got < 0) return FAILURE;
	file->offset += static_cast<uint32_t>(got);
	return static_cast<ssize_t>(got);
}

ssize_t __file_write(int fd, const void *buf, size_t count) {
	File *file = find(fd);
	if (file == nullptr || !file->writable || buf == nullptr) return FAILURE;

	if (file->append) {
		cock::user::fs::FileInfo info = {};
		if (!client.stat(file->handle, info)) return FAILURE;
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
	if (done == NO_BYTES && count != NO_BYTES) return FAILURE;
	return static_cast<ssize_t>(done);
}

off_t lseek(int fd, off_t offset, int whence) {
	File *file = find(fd);
	if (file == nullptr) return FAILURE;

	int64_t base = 0;
	if (whence == SEEK_CUR) {
		base = file->offset;
	} else if (whence == SEEK_END) {
		cock::user::fs::FileInfo info = {};
		if (!client.stat(file->handle, info)) return FAILURE;
		base = info.size;
	} else if (whence != SEEK_SET) {
		return FAILURE;
	}

	int64_t target = base + offset;
	if (target < static_cast<int64_t>(START_OFFSET) || target > static_cast<int64_t>(MAX_OFFSET))
		return FAILURE;
	file->offset = static_cast<uint32_t>(target);
	return static_cast<off_t>(target);
}

int unlink(const char *path) {
	if (path == nullptr || !ensureConnected()) return FAILURE;
	return client.unlink(path) ? SUCCESS : FAILURE;
}

}
