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

constexpr uint32_t OPEN_READ_ONLY = 0;
constexpr uint32_t NO_HANDLE = 0;
constexpr uint32_t NO_WINDOW = 0xFFFFFFFF;

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
};

enum class FileType : uint32_t {
	REGULAR = 0,
	DIRECTORY = 1,
	SYMLINK = 2,
};

static_assert(WINDOW_BYTES > MAX_PATH_BYTES, "the window must hold a path");

} // namespace cock::abi::fs

#endif // !SHARED_COCK_ABI_FS_HPP
