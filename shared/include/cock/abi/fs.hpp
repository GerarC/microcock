#ifndef SHARED_COCK_ABI_FS_HPP
#define SHARED_COCK_ABI_FS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::fs {

constexpr const char *SERVER_NAME = "vfs";
constexpr const char *RAMFS_SERVER_NAME = "ramfs";
constexpr const char *FAT_SERVER_NAME = "fat";
constexpr const char *INITRD_MODULE_NAME = "initrd";

constexpr size_t WINDOW_PAGE_BYTES = 0x1000;
constexpr size_t WINDOW_PAGES = 1;
constexpr size_t WINDOW_BYTES = WINDOW_PAGES * WINDOW_PAGE_BYTES;
constexpr size_t MAX_PATH_BYTES = 128;
constexpr size_t RENAME_NEW_OFFSET = MAX_PATH_BYTES;

constexpr uint32_t OPEN_READ_ONLY = 0;
constexpr uint32_t OPEN_WRITE = 1;
constexpr uint32_t OPEN_CREATE = 2;
constexpr uint32_t OPEN_TRUNCATE = 4;
constexpr uint32_t OPEN_FLAGS_MASK = OPEN_WRITE | OPEN_CREATE | OPEN_TRUNCATE;
constexpr uint32_t NO_HANDLE = 0;
constexpr uint32_t NO_WINDOW = 0xFFFFFFFF;
constexpr char PATH_SEPARATOR = '/';

enum class Status : uint32_t {
	OK = 0,
	NOT_CONNECTED = 1,
	BAD_REQUEST = 2,
	NOT_FOUND = 3,
	BAD_HANDLE = 4,
	OUT_OF_RANGE = 5,
	NO_SPACE = 6,
	UNSUPPORTED = 7,
	IO_ERROR = 8,
	READ_ONLY = 9,
	BUSY = 10,
	EXISTS = 11,
	NOT_EMPTY = 12,
	CROSS_DEVICE = 13,
};

enum class FileType : uint32_t {
	REGULAR = 0,
	DIRECTORY = 1,
	SYMLINK = 2,
};

static_assert(WINDOW_BYTES >= RENAME_NEW_OFFSET + MAX_PATH_BYTES, "the window must hold two paths");

inline bool splitPath(const char *path, char *parent, size_t parentCapacity,
					  const char *&leaf, size_t &leafBytes) {
	size_t end = 0;
	while (path[end] != '\0')
		end++;
	while (end > 0 && path[end - 1] == PATH_SEPARATOR)
		end--;

	size_t start = end;
	while (start > 0 && path[start - 1] != PATH_SEPARATOR)
		start--;
	if (start == end) return false;

	size_t parentEnd = start;
	while (parentEnd > 0 && path[parentEnd - 1] == PATH_SEPARATOR)
		parentEnd--;

	size_t parentBytes = parentEnd == 0 ? sizeof(char) : parentEnd;
	if (parentBytes + sizeof(char) > parentCapacity) return false;
	if (parentEnd == 0) {
		parent[0] = PATH_SEPARATOR;
	} else {
		for (size_t index = 0; index < parentEnd; index++)
			parent[index] = path[index];
	}
	parent[parentBytes] = '\0';

	leaf = path + start;
	leafBytes = end - start;
	return true;
}

} // namespace cock::abi::fs

#endif // !SHARED_COCK_ABI_FS_HPP
