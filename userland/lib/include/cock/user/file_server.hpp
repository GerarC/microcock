/**
 * @file file_server.hpp
 * @brief Generic file server loop shared by the ramfs, FAT32 and ext2 servers.
 *
 * `FileServer<Backend>` implements the `FS_*` message protocol: sessions per client with
 * a shared memory window, a table of open handles, open mode conflicts and the replies.
 * The backend supplies the on disk format. Everything is static state of one server
 * process and the loop is single threaded.
 *
 * Backend requirements: types `Node` (with `type`, `size`) and `Handle` (with `open`
 * and `node`), constant `MAX_FILE_BYTES`, and functions `writable`, `resolve`,
 * `resolveLeaf`, `create`, `truncate`, `read`, `write`, `list`, `unlink`,
 * `makeDirectory`, `removeDirectory`, `rename`, `link` and `sameFile`.
 *
 * Request layout (messages from clients, `arg0` is the request id): paths travel in the
 * shared window, the message carries their length. Every request is answered with an
 * `FS_REPLY` carrying the id, a `Status` and up to two result words.
 */
#ifndef COCK_USER_FILE_SERVER_HPP
#define COCK_USER_FILE_SERVER_HPP

#include <cock/abi/endpoint.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/session.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::user {

/**
 * @brief File server over a storage backend.
 * @tparam Backend Storage format; see the file description for what it must provide.
 */
template <typename Backend>
struct FileServer {
	/** Backend description of a file or directory. */
	using Node = typename Backend::Node;
	/** Backend state of an open file. */
	using Handle = typename Backend::Handle;
	/** Result code of a request. */
	using Status = abi::fs::Status;
	/** Kind of an entry (regular, directory). */
	using FileType = abi::fs::FileType;
	/** IPC message. */
	using Message = ipc::Message;
	/** IPC message type. */
	using MessageType = ipc::MessageType;

	/** Most clients with a session at once. */
	static constexpr size_t MAX_CLIENTS = 8;
	/** Most open handles per client. */
	static constexpr size_t MAX_HANDLES = 16;
	/** Capacity of the queue of deferred messages. */
	static constexpr size_t MAX_PENDING = 16;
	/** Index of the oldest deferred message. */
	static constexpr size_t FIRST_PENDING = 0;
	/** Zero byte count. */
	static constexpr uint32_t NO_BYTES = 0;

	/** State kept for one connected client. */
	struct Session {
		/** true when the slot belongs to a client. */
		bool used;
		/** Process id of the client. */
		uint32_t pid;
		/** Shared memory object id of the data window. */
		uint32_t windowId;
		/** Mapping of the data window in this server. */
		uint8_t *window;
		/** Open files; handle number is index plus one. */
		Handle handles[MAX_HANDLES];
		/** true for the handles opened for writing. */
		bool writers[MAX_HANDLES];
	};

	/** Session table. */
	inline static Session sessions[MAX_CLIENTS];
	/** Messages put aside with `defer` that `run` handles before reading new ones. */
	inline static Message pending[MAX_PENDING];
	/** Number of valid entries in `pending`. */
	inline static size_t pendingCount = 0;

	/**
	 * @brief Queues a message to be handled by `run` before any new one.
	 *
	 * Used by backends that receive other messages (for example block replies) while
	 * they wait. When the queue is full the message is dropped.
	 * @param message Message to keep.
	 */
	static void defer(const Message &message) {
		if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
	}

	/**
	 * @brief Serves requests forever. Never returns.
	 *
	 * Takes deferred messages first, then blocks in `ipc_recv`. Each message goes to `handle`.
	 */
	[[noreturn]] static void run() {
		Message message = {};
		for (;;) {
			if (pendingCount > 0) {
				message = pending[FIRST_PENDING];
				pendingCount--;
				for (size_t index = 0; index < pendingCount; index++)
					pending[index] = pending[index + 1];
			} else {
				ipc_recv(message);
			}
			handle(message);
		}
	}

	/**
	 * @brief Dispatches one message.
	 *
	 * Ignores messages from the kernel and unknown types. `FS_CONNECT` opens a session.
	 * Any other `FS_*` request from a client without a session is answered
	 * `NOT_CONNECTED`; otherwise it goes to the matching handler.
	 * @param message Message received.
	 */
	static void handle(const Message &message) {
		if (message.sender_pid == ipc::KERNEL_SENDER) return;
		if (message.reply == abi::endpoint::NO_REPLY) return;
		uint32_t token = message.reply;
		uint32_t client = message.badge;
		uint32_t id = message.payload.args.arg0;

		switch (message.type) {
		case MessageType::FS_CONNECT:
			return connect(token, client, id);
		case MessageType::FS_OPEN:
		case MessageType::FS_READ:
		case MessageType::FS_WRITE:
		case MessageType::FS_UNLINK:
		case MessageType::FS_MKDIR:
		case MessageType::FS_RMDIR:
		case MessageType::FS_RENAME:
		case MessageType::FS_LINK:
		case MessageType::FS_STAT:
		case MessageType::FS_READDIR:
		case MessageType::FS_CLOSE:
			break;
		default:
			return;
		}

		Session *session = findSession(sessions, MAX_CLIENTS, client);
		if (session == nullptr) return reply(token, id, Status::NOT_CONNECTED);

		switch (message.type) {
		case MessageType::FS_OPEN:
			return openFile(*session, message);
		case MessageType::FS_READ:
			return readFile(*session, message);
		case MessageType::FS_WRITE:
			return writeFile(*session, message);
		case MessageType::FS_UNLINK:
			return unlinkFile(*session, message);
		case MessageType::FS_MKDIR:
			return makeDirectory(*session, message);
		case MessageType::FS_RMDIR:
			return removeDirectory(*session, message);
		case MessageType::FS_RENAME:
			return renameEntry(*session, message);
		case MessageType::FS_LINK:
			return linkEntry(*session, message);
		case MessageType::FS_STAT:
			return statFile(*session, message);
		case MessageType::FS_READDIR:
			return listDirectory(*session, message);
		default:
			return closeFile(*session, message);
		}
	}

	/**
	 * @brief Sends an `FS_REPLY` through the reply token of a request. Never blocks; a full
	 *        inbox loses the reply.
	 * @param token Reply token the request carried.
	 * @param id Request id echoed back.
	 * @param status Result code.
	 * @param first First result word.
	 * @param second Second result word.
	 */
	static void reply(uint32_t token, uint32_t id, Status status, uint32_t first = 0,
					  uint32_t second = 0) {
		Message answer = {};
		answer.type = MessageType::FS_REPLY;
		answer.payload.args.arg0 = id;
		answer.payload.args.arg1 = static_cast<uint32_t>(status);
		answer.payload.args.arg2 = first;
		answer.payload.args.arg3 = second;
		ep_reply(token, answer);
	}

	/** @brief Frees the session and window of every client whose process is no longer alive. */
	static void releaseDeadSessions() {
		for (Session &session : sessions)
			if (session.used && !process_alive(session.pid))
				releaseSession(session, abi::fs::WINDOW_BYTES);
	}

	/**
	 * @brief Handles `FS_CONNECT`: creates a session and its shared window.
	 *
	 * A client that connects again keeps its session and window but loses its open
	 * handles. When the table is full, sessions of dead clients are freed first.
	 * @param token Reply token of the request.
	 * @param client Badge of the client, which is its process id.
	 * @param id Request id.
	 *
	 * Replies `OK` with the window object id in the first word, or `NO_SPACE` when
	 * there is no free slot or the window could not be created or shared.
	 */
	static void connect(uint32_t token, uint32_t client, uint32_t id) {
		Session *session = findSession(sessions, MAX_CLIENTS, client);
		if (session != nullptr) {
			memset(session->handles, 0, sizeof(session->handles));
			memset(session->writers, 0, sizeof(session->writers));
			return reply(token, id, Status::OK, session->windowId);
		}

		releaseDeadSessions();
		session = freeSession(sessions, MAX_CLIENTS);
		if (session == nullptr || !openSession(*session, client, abi::fs::WINDOW_PAGES))
			return reply(token, id, Status::NO_SPACE);
		reply(token, id, Status::OK, session->windowId);
	}

	/**
	 * @brief Looks up an open handle of a session.
	 * @param session Client session.
	 * @param number Handle number as given to the client (index plus one).
	 * @return The handle, or nullptr when the number is `NO_HANDLE`, out of range or closed.
	 */
	static Handle *findHandle(Session &session, uint32_t number) {
		if (number == abi::fs::NO_HANDLE || number > MAX_HANDLES) return nullptr;
		Handle &found = session.handles[number - 1];
		return found.open ? &found : nullptr;
	}

	/**
	 * @brief Copies a path from the client window into a local, NUL terminated buffer.
	 * @param session Client session.
	 * @param at Byte offset of the path in the window.
	 * @param length Path length claimed by the client; must be 1 to `MAX_PATH_BYTES` minus 1.
	 * @param path Destination of at least `MAX_PATH_BYTES` bytes.
	 * @return true when the length is valid and the path has no embedded NUL; false otherwise.
	 */
	static bool copyPath(const Session &session, size_t at, uint32_t length, char *path) {
		if (length == 0 || length >= abi::fs::MAX_PATH_BYTES) return false;
		memcpy(path, session.window + at, length);
		path[length] = '\0';
		return strlen(path) == length;
	}

	/**
	 * @brief Tells whether opening a node would clash with handles already open.
	 *
	 * Only regular files clash. Many readers are allowed; a writer excludes everyone.
	 * @param node File to open.
	 * @param wantsWrite true when the new handle would write.
	 * @return true when another handle on the same file is a writer or the new one writes.
	 */
	static bool conflicts(const Node &node, bool wantsWrite) {
		if (node.type != FileType::REGULAR) return false;
		for (const Session &other : sessions) {
			if (!other.used) continue;
			for (size_t slot = 0; slot < MAX_HANDLES; slot++) {
				const Handle &candidate = other.handles[slot];
				if (!candidate.open || candidate.node.type != FileType::REGULAR) continue;
				if (!Backend::sameFile(candidate.node, node)) continue;
				if (wantsWrite || other.writers[slot]) return true;
			}
		}
		return false;
	}

	/**
	 * @brief Handles `FS_OPEN`.
	 *
	 * Request: `arg1` path length (path at offset 0 of the window), `arg2` open flags.
	 * Reply: `OK` with the handle number (first word). Errors: `UNSUPPORTED` (unknown
	 * flags, or write on a directory), `BAD_REQUEST` (create or truncate without write,
	 * bad path), `READ_ONLY` (write on a read only backend), `NO_SPACE` (no free handle),
	 * `NOT_FOUND`, `BUSY` (clash with an open handle), or the backend error from
	 * create or truncate.
	 * @param session Client session.
	 * @param message Request.
	 */
	static void openFile(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		uint32_t length = message.payload.args.arg1;
		uint32_t flags = message.payload.args.arg2;
		if ((flags & ~abi::fs::OPEN_FLAGS_MASK) != 0)
			return reply(token, id, Status::UNSUPPORTED);

		bool wantsWrite = (flags & abi::fs::OPEN_WRITE) != 0;
		bool creates = (flags & abi::fs::OPEN_CREATE) != 0;
		bool truncates = (flags & abi::fs::OPEN_TRUNCATE) != 0;
		if ((creates || truncates) && !wantsWrite) return reply(token, id, Status::BAD_REQUEST);
		if (wantsWrite && !Backend::writable()) return reply(token, id, Status::READ_ONLY);

		char path[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, 0, length, path)) return reply(token, id, Status::BAD_REQUEST);

		size_t slot = 0;
		while (slot < MAX_HANDLES && session.handles[slot].open)
			slot++;
		if (slot == MAX_HANDLES) return reply(token, id, Status::NO_SPACE);

		Node node = {};
		if (!Backend::resolve(path, node)) {
			if (!creates) return reply(token, id, Status::NOT_FOUND);
			Status made = Backend::create(path, node);
			if (made != Status::OK) return reply(token, id, made);
		} else {
			if (wantsWrite && node.type != FileType::REGULAR)
				return reply(token, id, Status::UNSUPPORTED);
			if (conflicts(node, wantsWrite)) return reply(token, id, Status::BUSY);
			if (truncates && node.size != NO_BYTES) {
				Status cut = Backend::truncate(node);
				if (cut != Status::OK) return reply(token, id, cut);
			}
		}

		session.handles[slot] = Handle{};
		session.handles[slot].open = true;
		session.handles[slot].node = node;
		session.writers[slot] = wantsWrite;
		reply(token, id, Status::OK, static_cast<uint32_t>(slot + 1));
	}

	/**
	 * @brief Handles `FS_WRITE`.
	 *
	 * Request: `arg1` handle, `arg2` offset, `arg3` byte count (data at offset 0 of the
	 * window). The count is cut to the window size and to the room left below
	 * `MAX_FILE_BYTES`. Reply: `OK` with the bytes written. Errors: `BAD_HANDLE`,
	 * `READ_ONLY` (handle not opened for writing), `OUT_OF_RANGE` (offset past the end,
	 * so holes are not allowed), `NO_SPACE` (file at its maximum size), or the backend error.
	 * @param session Client session.
	 * @param message Request.
	 */
	static void writeFile(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		uint32_t number = message.payload.args.arg1;
		Handle *found = findHandle(session, number);
		if (found == nullptr) return reply(token, id, Status::BAD_HANDLE);
		if (!session.writers[number - 1]) return reply(token, id, Status::READ_ONLY);

		uint32_t offset = message.payload.args.arg2;
		uint32_t count = message.payload.args.arg3;
		if (offset > found->node.size) return reply(token, id, Status::OUT_OF_RANGE);
		if (count > abi::fs::WINDOW_BYTES) count = static_cast<uint32_t>(abi::fs::WINDOW_BYTES);
		if (count == NO_BYTES) return reply(token, id, Status::OK, NO_BYTES);

		uint32_t room = Backend::MAX_FILE_BYTES - offset;
		if (count > room) count = room;
		if (count == NO_BYTES) return reply(token, id, Status::NO_SPACE);

		uint32_t written = NO_BYTES;
		Status status = Backend::write(*found, session.window, offset, count, written);
		reply(token, id, status, written);
	}

	/**
	 * @brief Tells whether any open handle of any session refers to a node.
	 * @param node Node to look for.
	 * @return true when at least one open handle is on the same file.
	 */
	static bool inUse(const Node &node) {
		for (const Session &other : sessions) {
			if (!other.used) continue;
			for (size_t slot = 0; slot < MAX_HANDLES; slot++)
				if (other.handles[slot].open && Backend::sameFile(other.handles[slot].node, node))
					return true;
		}
		return false;
	}

	/**
	 * @brief Handles `FS_MKDIR`.
	 *
	 * Request: `arg1` path length (path at offset 0). Reply: the backend status.
	 * Errors: `READ_ONLY`, `BAD_REQUEST` (bad path).
	 * @param session Client session.
	 * @param message Request.
	 */
	static void makeDirectory(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		if (!Backend::writable()) return reply(token, id, Status::READ_ONLY);

		char path[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, 0, message.payload.args.arg1, path))
			return reply(token, id, Status::BAD_REQUEST);
		reply(token, id, Backend::makeDirectory(path));
	}

	/**
	 * @brief Handles `FS_RMDIR`.
	 *
	 * Request: `arg1` path length (path at offset 0). Reply: the backend status.
	 * Errors: `READ_ONLY`, `BAD_REQUEST`, `NOT_FOUND`, `UNSUPPORTED` (not a directory),
	 * `BUSY` (a handle is open on it).
	 * @param session Client session.
	 * @param message Request.
	 */
	static void removeDirectory(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		if (!Backend::writable()) return reply(token, id, Status::READ_ONLY);

		char path[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, 0, message.payload.args.arg1, path))
			return reply(token, id, Status::BAD_REQUEST);

		Node node = {};
		if (!Backend::resolveLeaf(path, node)) return reply(token, id, Status::NOT_FOUND);
		if (node.type != FileType::DIRECTORY) return reply(token, id, Status::UNSUPPORTED);
		if (inUse(node)) return reply(token, id, Status::BUSY);
		reply(token, id, Backend::removeDirectory(path));
	}

	/**
	 * @brief Handles `FS_RENAME`.
	 *
	 * Request: `arg1` length of the old path (at offset 0), `arg2` length of the new path
	 * (at `RENAME_NEW_OFFSET`). Reply: the backend status. Errors: `READ_ONLY`,
	 * `BAD_REQUEST`, `NOT_FOUND`, `BUSY` (the source or an existing target is open).
	 * @param session Client session.
	 * @param message Request.
	 */
	static void renameEntry(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		if (!Backend::writable()) return reply(token, id, Status::READ_ONLY);

		char from[abi::fs::MAX_PATH_BYTES] = {};
		char to[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, 0, message.payload.args.arg1, from) ||
			!copyPath(session, abi::fs::RENAME_NEW_OFFSET, message.payload.args.arg2, to))
			return reply(token, id, Status::BAD_REQUEST);

		Node node = {};
		if (!Backend::resolveLeaf(from, node)) return reply(token, id, Status::NOT_FOUND);
		if (inUse(node)) return reply(token, id, Status::BUSY);
		Node target = {};
		if (Backend::resolveLeaf(to, target) && inUse(target)) return reply(token, id, Status::BUSY);
		reply(token, id, Backend::rename(from, to));
	}

	/**
	 * @brief Handles `FS_LINK` (hard link).
	 *
	 * Request: `arg1` length of the existing path (at offset 0), `arg2` length of the new
	 * name (at `RENAME_NEW_OFFSET`). Reply: the backend status. Errors: `READ_ONLY`,
	 * `BAD_REQUEST`, `NOT_FOUND`, `UNSUPPORTED` (not a regular file), `BUSY` (open).
	 * @param session Client session.
	 * @param message Request.
	 */
	static void linkEntry(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		if (!Backend::writable()) return reply(token, id, Status::READ_ONLY);

		char existing[abi::fs::MAX_PATH_BYTES] = {};
		char name[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, 0, message.payload.args.arg1, existing) ||
			!copyPath(session, abi::fs::RENAME_NEW_OFFSET, message.payload.args.arg2, name))
			return reply(token, id, Status::BAD_REQUEST);

		Node node = {};
		if (!Backend::resolveLeaf(existing, node)) return reply(token, id, Status::NOT_FOUND);
		if (node.type != FileType::REGULAR) return reply(token, id, Status::UNSUPPORTED);
		if (inUse(node)) return reply(token, id, Status::BUSY);
		reply(token, id, Backend::link(existing, name));
	}

	/**
	 * @brief Handles `FS_UNLINK`.
	 *
	 * Request: `arg1` path length (path at offset 0). Reply: the backend status.
	 * Errors: `READ_ONLY`, `BAD_REQUEST`, `NOT_FOUND`, `UNSUPPORTED` (not a regular
	 * file), `BUSY` (any handle is open on it).
	 * @param session Client session.
	 * @param message Request.
	 */
	static void unlinkFile(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		if (!Backend::writable()) return reply(token, id, Status::READ_ONLY);

		char path[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, 0, message.payload.args.arg1, path))
			return reply(token, id, Status::BAD_REQUEST);

		Node node = {};
		if (!Backend::resolveLeaf(path, node)) return reply(token, id, Status::NOT_FOUND);
		if (node.type != FileType::REGULAR) return reply(token, id, Status::UNSUPPORTED);
		if (conflicts(node, true)) return reply(token, id, Status::BUSY);
		reply(token, id, Backend::unlink(path));
	}

	/**
	 * @brief Handles `FS_READ`.
	 *
	 * Request: `arg1` handle, `arg2` offset, `arg3` byte count. The data is left at
	 * offset 0 of the window. The count is cut to the file size and the window size.
	 * Reply: `OK` with the bytes read (zero at end of file). Errors: `BAD_HANDLE`,
	 * `UNSUPPORTED` (a directory), `OUT_OF_RANGE` (offset past the end), `IO_ERROR`.
	 * @param session Client session.
	 * @param message Request.
	 */
	static void readFile(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(token, id, Status::BAD_HANDLE);
		if (found->node.type != FileType::REGULAR)
			return reply(token, id, Status::UNSUPPORTED);

		uint32_t offset = message.payload.args.arg2;
		uint32_t length = message.payload.args.arg3;
		if (offset > found->node.size) return reply(token, id, Status::OUT_OF_RANGE);

		uint32_t count = found->node.size - offset;
		if (count > length) count = length;
		if (count > abi::fs::WINDOW_BYTES) count = static_cast<uint32_t>(abi::fs::WINDOW_BYTES);
		if (count == NO_BYTES) return reply(token, id, Status::OK, NO_BYTES);

		if (!Backend::read(*found, session.window, offset, count))
			return reply(token, id, Status::IO_ERROR);
		reply(token, id, Status::OK, count);
	}

	/**
	 * @brief Handles `FS_STAT`.
	 *
	 * Request: `arg1` handle. Reply: `OK` with the size in the first word and the file
	 * type in the second, or `BAD_HANDLE`.
	 * @param session Client session.
	 * @param message Request.
	 */
	static void statFile(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(token, id, Status::BAD_HANDLE);
		reply(token, id, Status::OK, found->node.size, static_cast<uint32_t>(found->node.type));
	}

	/**
	 * @brief Handles `FS_READDIR`.
	 *
	 * Request: `arg1` handle of a directory, `arg2` entry index. The NUL terminated name
	 * is left at offset 0 of the window. Reply: `OK` with the name length and the entry
	 * type. Errors: `BAD_HANDLE`, `UNSUPPORTED` (not a directory), `OUT_OF_RANGE` (no
	 * entry with that index, which ends the listing).
	 * @param session Client session.
	 * @param message Request.
	 */
	static void listDirectory(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(token, id, Status::BAD_HANDLE);
		if (found->node.type != FileType::DIRECTORY)
			return reply(token, id, Status::UNSUPPORTED);

		FileType type = FileType::REGULAR;
		char *name = reinterpret_cast<char *>(session.window);
		if (!Backend::list(found->node, message.payload.args.arg2, name,
						   abi::fs::WINDOW_BYTES, type))
			return reply(token, id, Status::OUT_OF_RANGE);
		reply(token, id, Status::OK, static_cast<uint32_t>(strlen(name)),
			  static_cast<uint32_t>(type));
	}

	/**
	 * @brief Handles `FS_CLOSE`.
	 *
	 * Request: `arg1` handle. Reply: `OK`, or `BAD_HANDLE`. The slot becomes free.
	 * @param session Client session.
	 * @param message Request.
	 */
	static void closeFile(Session &session, const Message &message) {
		uint32_t token = message.reply;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(token, id, Status::BAD_HANDLE);
		found->open = false;
		session.writers[message.payload.args.arg1 - 1] = false;
		reply(token, id, Status::OK);
	}
};

} // namespace cock::user

#endif // !COCK_USER_FILE_SERVER_HPP
