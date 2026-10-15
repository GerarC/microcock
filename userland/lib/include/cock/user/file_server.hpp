#ifndef COCK_USER_FILE_SERVER_HPP
#define COCK_USER_FILE_SERVER_HPP

#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/session.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::user {

template <typename Backend>
struct FileServer {
	using Node = typename Backend::Node;
	using Handle = typename Backend::Handle;
	using Status = abi::fs::Status;
	using FileType = abi::fs::FileType;
	using Message = ipc::Message;
	using MessageType = ipc::MessageType;

	static constexpr size_t MAX_CLIENTS = 8;
	static constexpr size_t MAX_HANDLES = 16;
	static constexpr size_t MAX_PENDING = 16;
	static constexpr size_t FIRST_PENDING = 0;
	static constexpr uint32_t NO_BYTES = 0;

	struct Session {
		bool used;
		uint32_t pid;
		uint32_t windowId;
		uint8_t *window;
		Handle handles[MAX_HANDLES];
		bool writers[MAX_HANDLES];
	};

	inline static Session sessions[MAX_CLIENTS];
	inline static Message pending[MAX_PENDING];
	inline static size_t pendingCount = 0;

	static void defer(const Message &message) {
		if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
	}

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

	static void handle(const Message &message) {
		if (message.sender_pid == ipc::KERNEL_SENDER) return;
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;

		switch (message.type) {
		case MessageType::FS_CONNECT:
			return connect(pid, id);
		case MessageType::FS_OPEN:
		case MessageType::FS_READ:
		case MessageType::FS_WRITE:
		case MessageType::FS_UNLINK:
		case MessageType::FS_STAT:
		case MessageType::FS_READDIR:
		case MessageType::FS_CLOSE:
			break;
		default:
			return;
		}

		Session *session = findSession(sessions, MAX_CLIENTS, pid);
		if (session == nullptr) return reply(pid, id, Status::NOT_CONNECTED);

		switch (message.type) {
		case MessageType::FS_OPEN:
			return openFile(*session, message);
		case MessageType::FS_READ:
			return readFile(*session, message);
		case MessageType::FS_WRITE:
			return writeFile(*session, message);
		case MessageType::FS_UNLINK:
			return unlinkFile(*session, message);
		case MessageType::FS_STAT:
			return statFile(*session, message);
		case MessageType::FS_READDIR:
			return listDirectory(*session, message);
		default:
			return closeFile(*session, message);
		}
	}

	static void reply(uint32_t pid, uint32_t id, Status status, uint32_t first = 0,
					  uint32_t second = 0) {
		Message answer = {};
		answer.type = MessageType::FS_REPLY;
		answer.payload.args.arg0 = id;
		answer.payload.args.arg1 = static_cast<uint32_t>(status);
		answer.payload.args.arg2 = first;
		answer.payload.args.arg3 = second;
		ipc_send(pid, answer);
	}

	static void releaseDeadSessions() {
		for (Session &session : sessions)
			if (session.used && !process_alive(session.pid))
				releaseSession(session, abi::fs::WINDOW_BYTES);
	}

	static void connect(uint32_t pid, uint32_t id) {
		Session *session = findSession(sessions, MAX_CLIENTS, pid);
		if (session != nullptr) {
			memset(session->handles, 0, sizeof(session->handles));
			memset(session->writers, 0, sizeof(session->writers));
			return reply(pid, id, Status::OK, session->windowId);
		}

		releaseDeadSessions();
		session = freeSession(sessions, MAX_CLIENTS);
		if (session == nullptr || !openSession(*session, pid, abi::fs::WINDOW_PAGES))
			return reply(pid, id, Status::NO_SPACE);
		reply(pid, id, Status::OK, session->windowId);
	}

	static Handle *findHandle(Session &session, uint32_t number) {
		if (number == abi::fs::NO_HANDLE || number > MAX_HANDLES) return nullptr;
		Handle &found = session.handles[number - 1];
		return found.open ? &found : nullptr;
	}

	static bool copyPath(const Session &session, uint32_t length, char *path) {
		if (length == 0 || length >= abi::fs::MAX_PATH_BYTES) return false;
		memcpy(path, session.window, length);
		path[length] = '\0';
		return strlen(path) == length;
	}

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

	static void openFile(Session &session, const Message &message) {
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;
		uint32_t length = message.payload.args.arg1;
		uint32_t flags = message.payload.args.arg2;
		if ((flags & ~abi::fs::OPEN_FLAGS_MASK) != 0)
			return reply(pid, id, Status::UNSUPPORTED);

		bool wantsWrite = (flags & abi::fs::OPEN_WRITE) != 0;
		bool creates = (flags & abi::fs::OPEN_CREATE) != 0;
		bool truncates = (flags & abi::fs::OPEN_TRUNCATE) != 0;
		if ((creates || truncates) && !wantsWrite) return reply(pid, id, Status::BAD_REQUEST);
		if (wantsWrite && !Backend::writable()) return reply(pid, id, Status::READ_ONLY);

		char path[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, length, path)) return reply(pid, id, Status::BAD_REQUEST);

		size_t slot = 0;
		while (slot < MAX_HANDLES && session.handles[slot].open)
			slot++;
		if (slot == MAX_HANDLES) return reply(pid, id, Status::NO_SPACE);

		Node node = {};
		if (!Backend::resolve(path, node)) {
			if (!creates) return reply(pid, id, Status::NOT_FOUND);
			Status made = Backend::create(path, node);
			if (made != Status::OK) return reply(pid, id, made);
		} else {
			if (wantsWrite && node.type != FileType::REGULAR)
				return reply(pid, id, Status::UNSUPPORTED);
			if (conflicts(node, wantsWrite)) return reply(pid, id, Status::BUSY);
			if (truncates && node.size != NO_BYTES) {
				Status cut = Backend::truncate(node);
				if (cut != Status::OK) return reply(pid, id, cut);
			}
		}

		session.handles[slot] = Handle{};
		session.handles[slot].open = true;
		session.handles[slot].node = node;
		session.writers[slot] = wantsWrite;
		reply(pid, id, Status::OK, static_cast<uint32_t>(slot + 1));
	}

	static void writeFile(Session &session, const Message &message) {
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;
		uint32_t number = message.payload.args.arg1;
		Handle *found = findHandle(session, number);
		if (found == nullptr) return reply(pid, id, Status::BAD_HANDLE);
		if (!session.writers[number - 1]) return reply(pid, id, Status::READ_ONLY);

		uint32_t offset = message.payload.args.arg2;
		uint32_t count = message.payload.args.arg3;
		if (offset > found->node.size) return reply(pid, id, Status::OUT_OF_RANGE);
		if (count > abi::fs::WINDOW_BYTES) count = static_cast<uint32_t>(abi::fs::WINDOW_BYTES);
		if (count == NO_BYTES) return reply(pid, id, Status::OK, NO_BYTES);

		uint32_t room = Backend::MAX_FILE_BYTES - offset;
		if (count > room) count = room;
		if (count == NO_BYTES) return reply(pid, id, Status::NO_SPACE);

		uint32_t written = NO_BYTES;
		Status status = Backend::write(*found, session.window, offset, count, written);
		reply(pid, id, status, written);
	}

	static void unlinkFile(Session &session, const Message &message) {
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;
		if (!Backend::writable()) return reply(pid, id, Status::READ_ONLY);

		char path[abi::fs::MAX_PATH_BYTES] = {};
		if (!copyPath(session, message.payload.args.arg1, path))
			return reply(pid, id, Status::BAD_REQUEST);

		Node node = {};
		if (!Backend::resolveLeaf(path, node)) return reply(pid, id, Status::NOT_FOUND);
		if (node.type != FileType::REGULAR) return reply(pid, id, Status::UNSUPPORTED);
		if (conflicts(node, true)) return reply(pid, id, Status::BUSY);
		reply(pid, id, Backend::unlink(path));
	}

	static void readFile(Session &session, const Message &message) {
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(pid, id, Status::BAD_HANDLE);
		if (found->node.type != FileType::REGULAR)
			return reply(pid, id, Status::UNSUPPORTED);

		uint32_t offset = message.payload.args.arg2;
		uint32_t length = message.payload.args.arg3;
		if (offset > found->node.size) return reply(pid, id, Status::OUT_OF_RANGE);

		uint32_t count = found->node.size - offset;
		if (count > length) count = length;
		if (count > abi::fs::WINDOW_BYTES) count = static_cast<uint32_t>(abi::fs::WINDOW_BYTES);
		if (count == NO_BYTES) return reply(pid, id, Status::OK, NO_BYTES);

		if (!Backend::read(*found, session.window, offset, count))
			return reply(pid, id, Status::IO_ERROR);
		reply(pid, id, Status::OK, count);
	}

	static void statFile(Session &session, const Message &message) {
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(pid, id, Status::BAD_HANDLE);
		reply(pid, id, Status::OK, found->node.size, static_cast<uint32_t>(found->node.type));
	}

	static void listDirectory(Session &session, const Message &message) {
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(pid, id, Status::BAD_HANDLE);
		if (found->node.type != FileType::DIRECTORY)
			return reply(pid, id, Status::UNSUPPORTED);

		FileType type = FileType::REGULAR;
		char *name = reinterpret_cast<char *>(session.window);
		if (!Backend::list(found->node, message.payload.args.arg2, name,
						   abi::fs::WINDOW_BYTES, type))
			return reply(pid, id, Status::OUT_OF_RANGE);
		reply(pid, id, Status::OK, static_cast<uint32_t>(strlen(name)),
			  static_cast<uint32_t>(type));
	}

	static void closeFile(Session &session, const Message &message) {
		uint32_t pid = message.sender_pid;
		uint32_t id = message.payload.args.arg0;
		Handle *found = findHandle(session, message.payload.args.arg1);
		if (found == nullptr) return reply(pid, id, Status::BAD_HANDLE);
		found->open = false;
		session.writers[message.payload.args.arg1 - 1] = false;
		reply(pid, id, Status::OK);
	}
};

} // namespace cock::user

#endif // !COCK_USER_FILE_SERVER_HPP
