/**
 * @file fs.hpp
 * @brief Client of a file server (ramfs, FAT32, ext2 or vfs) over asynchronous messages.
 *
 * Requests are `FS_*` messages with an id; replies are `FS_REPLY`. Paths and file data
 * travel through a shared memory window that the server creates on `FS_CONNECT` and the
 * client maps. Every call blocks until the reply arrives or the server dies. After a
 * failed call `lastStatus()` tells why. One request is in flight at a time and the
 * object is not thread safe.
 */
#ifndef COCK_USER_FS_HPP
#define COCK_USER_FS_HPP

#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <cock/user/reply.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::user::fs {

using abi::fs::FileType;
using abi::fs::Status;
using ipc::Message;
using ipc::MessageType;

/** Value returned by `read` on failure. */
constexpr long READ_FAILED = -1;
/** Value returned by `write` on failure. */
constexpr long WRITE_FAILED = -1;

/** Decoded `FS_REPLY`. */
struct Reply {
	/** Result code given by the server. */
	Status status;
	/** First result word; depends on the request. */
	uint32_t first;
	/** Second result word; depends on the request. */
	uint32_t second;
};

/** Result of `stat`. */
struct FileInfo {
	/** Size in bytes (for a directory, the value the backend reports). */
	uint32_t size;
	/** Regular file or directory. */
	FileType type;
};

/** Connection to one file server. */
class Client {
  public:
	/**
	 * Called with every message that arrives while a reply is awaited and is not that
	 * reply. When null such messages are dropped.
	 */
	void (*onForeign)(const Message &) = nullptr;

	/**
	 * @brief Asks the name server for a file server, opens a session and maps its window.
	 *
	 * Connects through the name server (`names.hpp`), so the manifest must let the caller talk
	 * to the server. Then sends `FS_CONNECT` and maps the shared object, writable. Blocks for
	 * the replies.
	 * @param name Name of the server; the default is the main file server.
	 * @return true when connected; false when the name server refused or does not know the
	 *         server, the server refused (no free session slot), died, or the window could
	 *         not be mapped.
	 */
	bool connect(const char *name = abi::fs::SERVER_NAME) {
		return connectTo(names::server(name));
	}

	/**
	 * @brief Opens a session with a file server whose process id is known, and maps its window.
	 *
	 * For a caller that already holds the right to talk to the process: the root server, which
	 * reaches every process, and callers that connected through the name server themselves.
	 * @param pid Process id of the server.
	 * @return The same as `connect`.
	 */
	bool connectTo(uint32_t pid) {
		server = pid;
		if (server == NO_PROCESS) return false;

		Reply reply = {};
		if (!call(MessageType::FS_CONNECT, 0, 0, 0, reply) ||
			reply.status != Status::OK)
			return false;

		view = static_cast<uint8_t *>(shm_map(reply.first, true));
		return view != nullptr;
	}

	/**
	 * @brief Returns the status of the last call.
	 * @return The server status, `NOT_CONNECTED` when the call could not reach the server,
	 *         or `BAD_REQUEST` when a path was rejected on the client side.
	 */
	Status lastStatus() const { return status; }

	/**
	 * @brief Sends one request and waits for the reply with the same id (low level).
	 *
	 * Sets `lastStatus()` to `NOT_CONNECTED` first and to the reply status when one arrives.
	 * @param type Request type.
	 * @param first Request argument 1.
	 * @param second Request argument 2.
	 * @param third Request argument 3.
	 * @param reply Receives the decoded reply.
	 * @return true when a reply arrived (check `reply.status`); false when not connected,
	 *         the send failed or the server died while waiting.
	 */
	bool call(MessageType type, uint32_t first, uint32_t second, uint32_t third,
			  Reply &reply) {
		status = Status::NOT_CONNECTED;
		if (server == NO_PROCESS) return false;

		uint32_t id = nextId++;
		Message request = {};
		request.type = type;
		request.payload.args.arg0 = id;
		request.payload.args.arg1 = first;
		request.payload.args.arg2 = second;
		request.payload.args.arg3 = third;
		if (!ipc_send(server, request)) return false;

		Message answer = {};
		bool matches = receiveReply(
			server, answer,
			[&](const Message &candidate) {
				return candidate.type == MessageType::FS_REPLY &&
					   candidate.sender_pid == server &&
					   candidate.payload.args.arg0 == id;
			},
			onForeign);
		if (!matches) return false;

		reply.status = static_cast<Status>(answer.payload.args.arg1);
		status = reply.status;
		reply.first = answer.payload.args.arg2;
		reply.second = answer.payload.args.arg3;
		return true;
	}

	/**
	 * @brief Opens a file or directory (`FS_OPEN`). Blocks for the reply.
	 * @param path NUL terminated path, 1 to `MAX_PATH_BYTES` minus 1 bytes.
	 * @param flags Open flags (read only by default; write, create, truncate).
	 * @return The handle number, or `NO_HANDLE` on failure (see `lastStatus()`: `BAD_REQUEST`,
	 *         `NOT_FOUND`, `READ_ONLY`, `BUSY`, `NO_SPACE`, `UNSUPPORTED`, ...).
	 */
	uint32_t open(const char *path, uint32_t flags = abi::fs::OPEN_READ_ONLY) {
		size_t length = strlen(path);
		if (view == nullptr || length == 0 || length >= abi::fs::MAX_PATH_BYTES) {
			status = Status::BAD_REQUEST;
			return abi::fs::NO_HANDLE;
		}

		memcpy(view, path, length);
		view[length] = '\0';

		Reply reply = {};
		if (!call(MessageType::FS_OPEN, static_cast<uint32_t>(length), flags, 0, reply) ||
			reply.status != Status::OK)
			return abi::fs::NO_HANDLE;
		return reply.first;
	}

	/**
	 * @brief Writes bytes at an offset (`FS_WRITE`). Blocks for the reply.
	 *
	 * The length is cut to the window size, so the caller loops for larger writes. The
	 * offset cannot be past the end of the file.
	 * @param handle Handle opened for writing.
	 * @param offset Byte offset in the file.
	 * @param buffer Data to write.
	 * @param length Number of bytes.
	 * @return The bytes written (can be less than requested), or `WRITE_FAILED`
	 *         (see `lastStatus()`: `BAD_HANDLE`, `READ_ONLY`, `OUT_OF_RANGE`, `NO_SPACE`, ...).
	 */
	long write(uint32_t handle, uint32_t offset, const void *buffer, size_t length) {
		if (view == nullptr) {
			status = Status::NOT_CONNECTED;
			return WRITE_FAILED;
		}
		if (length > abi::fs::WINDOW_BYTES) length = abi::fs::WINDOW_BYTES;

		memcpy(view, buffer, length);
		Reply reply = {};
		if (!call(MessageType::FS_WRITE, handle, offset, static_cast<uint32_t>(length),
				  reply) ||
			reply.status != Status::OK || reply.first > length)
			return WRITE_FAILED;
		return static_cast<long>(reply.first);
	}

	/**
	 * @brief Removes a regular file (`FS_UNLINK`). Blocks for the reply.
	 * @param path NUL terminated path.
	 * @return true on success; false otherwise (`NOT_FOUND`, `BUSY`, `READ_ONLY`, ...).
	 */
	bool unlink(const char *path) { return pathCall(MessageType::FS_UNLINK, path); }

	/**
	 * @brief Creates a directory (`FS_MKDIR`). Blocks for the reply.
	 * @param path NUL terminated path.
	 * @return true on success; false otherwise (see `lastStatus()`).
	 */
	bool mkdir(const char *path) { return pathCall(MessageType::FS_MKDIR, path); }

	/**
	 * @brief Removes an empty directory (`FS_RMDIR`). Blocks for the reply.
	 * @param path NUL terminated path.
	 * @return true on success; false otherwise (`NOT_FOUND`, `BUSY`, `UNSUPPORTED`, ...).
	 */
	bool rmdir(const char *path) { return pathCall(MessageType::FS_RMDIR, path); }

	/**
	 * @brief Renames or moves an entry (`FS_RENAME`). Blocks for the reply.
	 * @param from Current path.
	 * @param to New path.
	 * @return true on success; false otherwise (`NOT_FOUND`, `BUSY`, `READ_ONLY`, ...).
	 */
	bool rename(const char *from, const char *to) {
		return twoPathCall(MessageType::FS_RENAME, from, to);
	}

	/**
	 * @brief Creates a hard link to a regular file (`FS_LINK`). Blocks for the reply.
	 * @param existing Path of the existing file.
	 * @param name Path of the new name.
	 * @return true on success; false otherwise (`NOT_FOUND`, `BUSY`, `UNSUPPORTED`, ...).
	 */
	bool link(const char *existing, const char *name) {
		return twoPathCall(MessageType::FS_LINK, existing, name);
	}

	/**
	 * @brief Sends a request that carries two paths (rename, link).
	 *
	 * The first path goes at offset 0 of the window, the second at `RENAME_NEW_OFFSET`.
	 * @param type Request type.
	 * @param from First path.
	 * @param to Second path.
	 * @return true when the server answered `OK`; false when a path is empty or too long
	 *         (status `BAD_REQUEST`), not connected, or the server failed.
	 */
	bool twoPathCall(MessageType type, const char *from, const char *to) {
		size_t fromLength = strlen(from);
		size_t toLength = strlen(to);
		if (view == nullptr || fromLength == 0 || fromLength >= abi::fs::MAX_PATH_BYTES ||
			toLength == 0 || toLength >= abi::fs::MAX_PATH_BYTES) {
			status = Status::BAD_REQUEST;
			return false;
		}

		memcpy(view, from, fromLength);
		view[fromLength] = '\0';
		memcpy(view + abi::fs::RENAME_NEW_OFFSET, to, toLength);
		view[abi::fs::RENAME_NEW_OFFSET + toLength] = '\0';

		Reply reply = {};
		return call(type, static_cast<uint32_t>(fromLength),
					static_cast<uint32_t>(toLength), 0, reply) &&
			   reply.status == Status::OK;
	}

	/**
	 * @brief Reads bytes at an offset (`FS_READ`). Blocks for the reply.
	 *
	 * The length is cut to the window size, so the caller loops for larger reads.
	 * @param handle Open handle of a regular file.
	 * @param offset Byte offset in the file.
	 * @param buffer Destination of at least `length` bytes.
	 * @param length Most bytes to read.
	 * @return The bytes read (zero at end of file), or `READ_FAILED` (see `lastStatus()`:
	 *         `BAD_HANDLE`, `OUT_OF_RANGE`, `UNSUPPORTED`, `IO_ERROR`, ...).
	 */
	long read(uint32_t handle, uint32_t offset, void *buffer, size_t length) {
		if (view == nullptr) {
			status = Status::NOT_CONNECTED;
			return READ_FAILED;
		}
		if (length > abi::fs::WINDOW_BYTES) length = abi::fs::WINDOW_BYTES;

		Reply reply = {};
		if (!call(MessageType::FS_READ, handle, offset, static_cast<uint32_t>(length),
				  reply) ||
			reply.status != Status::OK || reply.first > length)
			return READ_FAILED;

		memcpy(buffer, view, reply.first);
		return static_cast<long>(reply.first);
	}

	/**
	 * @brief Reads one entry name of a directory (`FS_READDIR`). Blocks for the reply.
	 * @param handle Open handle of a directory.
	 * @param index Entry index, starting at zero.
	 * @param name Destination for the NUL terminated name.
	 * @param size Capacity of `name`; the name must fit with its terminator.
	 * @param type Receives the type of the entry.
	 * @return true when an entry was returned; false at the end of the directory (the
	 *         usual loop end, `OUT_OF_RANGE`), for a bad handle, a name that does not fit
	 *         or a failed call.
	 */
	bool readdir(uint32_t handle, uint32_t index, char *name, size_t size,
				 FileType &type) {
		Reply reply = {};
		if (view == nullptr || size == 0 ||
			!call(MessageType::FS_READDIR, handle, index, 0, reply) ||
			reply.status != Status::OK || reply.first >= size ||
			reply.first >= abi::fs::WINDOW_BYTES)
			return false;

		memcpy(name, view, reply.first + sizeof(char));
		type = static_cast<FileType>(reply.second);
		return true;
	}

	/**
	 * @brief Gets size and type of an open file (`FS_STAT`). Blocks for the reply.
	 * @param handle Open handle.
	 * @param info Receives the size and type on success.
	 * @return true on success; false for a bad handle or a failed call.
	 */
	bool stat(uint32_t handle, FileInfo &info) {
		Reply reply = {};
		if (!call(MessageType::FS_STAT, handle, 0, 0, reply) ||
			reply.status != Status::OK)
			return false;

		info.size = reply.first;
		info.type = static_cast<FileType>(reply.second);
		return true;
	}

	/**
	 * @brief Closes a handle (`FS_CLOSE`). Blocks for the reply.
	 * @param handle Open handle.
	 * @return true on success; false for a bad handle or a failed call.
	 */
	bool close(uint32_t handle) {
		Reply reply = {};
		return call(MessageType::FS_CLOSE, handle, 0, 0, reply) &&
			   reply.status == Status::OK;
	}

  private:
	/**
	 * @brief Sends a request that carries one path (unlink, mkdir, rmdir).
	 * @param type Request type.
	 * @param path NUL terminated path, copied to offset 0 of the window.
	 * @return true when the server answered `OK`; false when the path is empty or too long
	 *         (status `BAD_REQUEST`), not connected, or the server failed.
	 */
	bool pathCall(MessageType type, const char *path) {
		size_t length = strlen(path);
		if (view == nullptr || length == 0 || length >= abi::fs::MAX_PATH_BYTES) {
			status = Status::BAD_REQUEST;
			return false;
		}

		memcpy(view, path, length);
		view[length] = '\0';

		Reply reply = {};
		return call(type, static_cast<uint32_t>(length), 0, 0, reply) &&
			   reply.status == Status::OK;
	}

	/** Process id of the server, `NO_PROCESS` before `connect`. */
	uint32_t server = NO_PROCESS;
	/** Mapping of the shared window, null before `connect`. */
	uint8_t *view = nullptr;
	/** Id of the next request. */
	uint32_t nextId = 1;
	/** Status of the last call. */
	Status status = Status::NOT_CONNECTED;
};

} // namespace cock::user::fs

#endif // !COCK_USER_FS_HPP
