/**
 * @file fs.hpp
 * @brief Wire protocol of the file servers (`vfs`, `ramfs`, `fat`, ext2) and path helpers.
 *
 * Requests are IPC messages (`FS_CONNECT`, `FS_OPEN`, `FS_READ`, `FS_WRITE`, `FS_STAT`, `FS_READDIR`,
 * `FS_CLOSE`, `FS_UNLINK`, `FS_MKDIR`, `FS_RMDIR`, `FS_RENAME`, `FS_LINK`) sent by a client to the process
 * named `SERVER_NAME`. `arg0` is a request id chosen by the client. The reply is `FS_REPLY` with the same
 * id in `arg0`, a `Status` in `arg1` and up to two values in `arg2` and `arg3`. Paths, names and file data
 * travel through a one page shared memory window created by the server on `FS_CONNECT`. Handles are opaque
 * numbers chosen by the server; `NO_HANDLE` is never valid. The `vfs` server routes each path to the file
 * system mounted on it and forwards the request to that server.
 */
#ifndef SHARED_COCK_ABI_FS_HPP
#define SHARED_COCK_ABI_FS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::fs {

/** Name of the virtual file system server that clients talk to. */
constexpr const char *SERVER_NAME = "vfs";
/** Name of the in-memory file system server (mounted from the initrd). */
constexpr const char *RAMFS_SERVER_NAME = "ramfs";
/** Name of the FAT32 file system server. */
constexpr const char *FAT_SERVER_NAME = "fat";
/** Name of the boot data module that holds the initrd archive. */
constexpr const char *INITRD_MODULE_NAME = "initrd";

/** Size of one page of the shared window in bytes. */
constexpr size_t WINDOW_PAGE_BYTES = 0x1000;
/** Pages of the shared window per client session. */
constexpr size_t WINDOW_PAGES = 1;
/** Size of the shared window in bytes (4096); the most a single read or write moves. */
constexpr size_t WINDOW_BYTES = WINDOW_PAGES * WINDOW_PAGE_BYTES;
/** Longest path in bytes, without the terminator; longer paths are `BAD_REQUEST`. */
constexpr size_t MAX_PATH_BYTES = 128;
/** Offset inside the window of the second path of `FS_RENAME` and `FS_LINK` (the first path is at offset 0). */
constexpr size_t RENAME_NEW_OFFSET = MAX_PATH_BYTES;

/** Open flags: read only, the empty set. */
constexpr uint32_t OPEN_READ_ONLY = 0;
/** Open flag: allow `FS_WRITE` on the handle. */
constexpr uint32_t OPEN_WRITE = 1;
/** Open flag: create the file if the path does not exist; needs `OPEN_WRITE`. */
constexpr uint32_t OPEN_CREATE = 2;
/** Open flag: empty an existing file; needs `OPEN_WRITE`. */
constexpr uint32_t OPEN_TRUNCATE = 4;
/** Set of all defined open flags; a bit outside it is `UNSUPPORTED`. */
constexpr uint32_t OPEN_FLAGS_MASK = OPEN_WRITE | OPEN_CREATE | OPEN_TRUNCATE;
/** Value of the second argument of `FS_CONNECT` that keeps the files the client already has open at the server: a program that replaced its image with `exec` keeps its process id, so its session and its files are still there. Any other value closes them, as a client that connects again expects. */
constexpr uint32_t CONNECT_KEEP_FILES = 1;
/** Handle value that never names a file; returned by clients on failure. */
constexpr uint32_t NO_HANDLE = 0;
/** Window id returned by `FS_CONNECT` when the window could not be created. */
constexpr uint32_t NO_WINDOW = 0xFFFFFFFF;
/** Separator of path components. */
constexpr char PATH_SEPARATOR = '/';

/** Result code carried in `arg1` of `FS_REPLY`. */
enum class Status : uint32_t {
	/** The request succeeded; the values in the reply are valid. */
	OK = 0,
	/** The sender has no session, or the server behind the `vfs` is unreachable. */
	NOT_CONNECTED = 1,
	/** Malformed request: bad length, bad flag combination or unknown message. */
	BAD_REQUEST = 2,
	/** The path (or its parent) does not exist. */
	NOT_FOUND = 3,
	/** The handle is not open in the session of the sender. */
	BAD_HANDLE = 4,
	/** Offset or index beyond the end of the file or directory. */
	OUT_OF_RANGE = 5,
	/** The volume or the server table is full. */
	NO_SPACE = 6,
	/** Operation not valid for this object type or file system. */
	UNSUPPORTED = 7,
	/** The block device or a lower server failed. */
	IO_ERROR = 8,
	/** The handle or the file system does not allow writing. */
	READ_ONLY = 9,
	/** The object is open or mounted and cannot be changed now. */
	BUSY = 10,
	/** The target name already exists. */
	EXISTS = 11,
	/** The directory has entries. */
	NOT_EMPTY = 12,
	/** Rename or link between two different file systems. */
	CROSS_DEVICE = 13,
};

/** Type of a file system object, the second value of `FS_STAT` and `FS_READDIR`. */
enum class FileType : uint32_t {
	/** Regular file. */
	REGULAR = 0,
	/** Directory. */
	DIRECTORY = 1,
	/** Symbolic link (only reported, never followed by the servers). */
	SYMLINK = 2,
};

static_assert(WINDOW_BYTES >= RENAME_NEW_OFFSET + MAX_PATH_BYTES, "the window must hold two paths");

/**
 * @brief Splits a path into its parent directory and its last component.
 *
 * Trailing separators are ignored. The parent of a single component is the root `/`.
 * @param path Null terminated path; must not be null.
 * @param parent Output buffer for the null terminated parent path.
 * @param parentCapacity Size of `parent` in bytes, including the terminator.
 * @param leaf Output: points inside `path` at the last component (not terminated at its end).
 * @param leafBytes Output: length of the last component.
 * @return False when the path has no component (empty or only separators) or the parent does not fit; true otherwise.
 */
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
