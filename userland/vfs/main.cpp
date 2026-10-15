#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/session.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::fs::FileType;
using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr size_t MAX_CLIENTS = 8;
constexpr size_t MAX_OPEN = 16;
constexpr size_t MAX_PENDING = 16;
constexpr size_t FIRST_PENDING = 0;
constexpr char PATH_SEPARATOR = '/';
constexpr size_t NOT_FOUND_MOUNT = static_cast<size_t>(-1);
constexpr size_t SEPARATOR_BYTES = 1;
constexpr uint32_t START_COUNT = 0;

struct Mount {
	const char *prefix;
	const char *server;
	Client client;
	bool connected;
};

struct Open {
	bool used;
	uint32_t owner;
	size_t mount;
	uint32_t backend;
};

struct Session {
	bool used;
	uint32_t pid;
	uint32_t windowId;
	uint8_t *window;
};

Mount mounts[] = {
	{"/", cock::abi::fs::RAMFS_SERVER_NAME, {}, false},
	{"/fat", cock::abi::fs::FAT_SERVER_NAME, {}, false},
};
constexpr size_t MOUNT_COUNT = sizeof(mounts) / sizeof(mounts[0]);

Session sessions[MAX_CLIENTS];
Open opens[MAX_OPEN];
Message pending[MAX_PENDING];
size_t pendingCount = 0;

void defer(const Message &message) {
	if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
}

bool startsWith(const char *text, const char *prefix, size_t length) {
	return strlen(prefix) == length && memcmp(text, prefix, length) == 0;
}

size_t pickMount(const char *full, const char *&rest) {
	size_t best = NOT_FOUND_MOUNT;
	size_t bestLength = 0;
	for (size_t index = 0; index < MOUNT_COUNT; index++) {
		size_t length = strlen(mounts[index].prefix);
		if (length < bestLength || strlen(full) < length) continue;
		if (!startsWith(full, mounts[index].prefix, length)) continue;

		bool whole = length == SEPARATOR_BYTES || full[length] == '\0' ||
					 full[length] == PATH_SEPARATOR;
		if (!whole) continue;
		best = index;
		bestLength = length;
	}

	if (best == NOT_FOUND_MOUNT) return best;
	rest = bestLength == SEPARATOR_BYTES ? full : full + bestLength;
	if (*rest == '\0') rest = mounts[0].prefix;
	return best;
}

bool ensureMount(size_t index) {
	Mount &mount = mounts[index];
	if (!mount.connected) {
		mount.client.onForeign = defer;
		mount.connected = mount.client.connect(mount.server);
	}
	return mount.connected;
}

void closeOpen(Open &open) {
	mounts[open.mount].client.close(open.backend);
	memset(&open, 0, sizeof(open));
}

void closeOwnedBy(uint32_t pid) {
	for (Open &open : opens)
		if (open.used && open.owner == pid) closeOpen(open);
}

void releaseDead() {
	for (Open &open : opens)
		if (open.used && !cock::user::process_alive(open.owner)) closeOpen(open);

	for (Session &session : sessions)
		if (session.used && !cock::user::process_alive(session.pid))
			cock::user::releaseSession(session, cock::abi::fs::WINDOW_BYTES);
}

void reply(uint32_t pid, uint32_t id, Status status, uint32_t first = 0,
		   uint32_t second = 0) {
	Message answer = {};
	answer.type = MessageType::FS_REPLY;
	answer.payload.args.arg0 = id;
	answer.payload.args.arg1 = static_cast<uint32_t>(status);
	answer.payload.args.arg2 = first;
	answer.payload.args.arg3 = second;
	cock::user::ipc_send(pid, answer);
}

Status failure(const Client &client) {
	return client.lastStatus() == Status::OK ? Status::IO_ERROR : client.lastStatus();
}

void connect(uint32_t pid, uint32_t id) {
	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, pid);
	if (session != nullptr) {
		closeOwnedBy(pid);
		return reply(pid, id, Status::OK, session->windowId);
	}

	releaseDead();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr ||
		!cock::user::openSession(*session, pid, cock::abi::fs::WINDOW_PAGES))
		return reply(pid, id, Status::NO_SPACE);
	reply(pid, id, Status::OK, session->windowId);
}

Open *findOpen(uint32_t pid, uint32_t number) {
	if (number == cock::abi::fs::NO_HANDLE || number > MAX_OPEN) return nullptr;
	Open &open = opens[number - 1];
	return open.used && open.owner == pid ? &open : nullptr;
}

Open *freeOpen() {
	for (Open &open : opens)
		if (!open.used) return &open;
	return nullptr;
}

void openFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	uint32_t length = message.payload.args.arg1;
	if (message.payload.args.arg2 != cock::abi::fs::OPEN_READ_ONLY)
		return reply(pid, id, Status::UNSUPPORTED);
	if (length == 0 || length >= cock::abi::fs::MAX_PATH_BYTES)
		return reply(pid, id, Status::BAD_REQUEST);

	char path[cock::abi::fs::MAX_PATH_BYTES] = {};
	memcpy(path, session.window, length);
	if (strlen(path) != length) return reply(pid, id, Status::BAD_REQUEST);

	char full[cock::abi::fs::MAX_PATH_BYTES + SEPARATOR_BYTES] = {};
	size_t offset = 0;
	if (path[0] != PATH_SEPARATOR) full[offset++] = PATH_SEPARATOR;
	memcpy(full + offset, path, length);

	const char *rest = nullptr;
	size_t index = pickMount(full, rest);
	if (index == NOT_FOUND_MOUNT || strlen(rest) >= cock::abi::fs::MAX_PATH_BYTES)
		return reply(pid, id, Status::BAD_REQUEST);
	if (!ensureMount(index)) return reply(pid, id, Status::NOT_FOUND);

	Open *slot = freeOpen();
	if (slot == nullptr) {
		releaseDead();
		slot = freeOpen();
	}
	if (slot == nullptr) return reply(pid, id, Status::NO_SPACE);

	Client &client = mounts[index].client;
	uint32_t backend = client.open(rest);
	if (backend == cock::abi::fs::NO_HANDLE) return reply(pid, id, failure(client));

	*slot = {true, pid, index, backend};
	reply(pid, id, Status::OK, static_cast<uint32_t>(slot - opens) + 1);
}

void readFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	Client &client = mounts[open->mount].client;
	long count = client.read(open->backend, message.payload.args.arg2, session.window,
							 message.payload.args.arg3);
	if (count < 0) return reply(pid, id, failure(client));
	reply(pid, id, Status::OK, static_cast<uint32_t>(count));
}

void statFile(const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	Client &client = mounts[open->mount].client;
	FileInfo info = {};
	if (!client.stat(open->backend, info)) return reply(pid, id, failure(client));
	reply(pid, id, Status::OK, info.size, static_cast<uint32_t>(info.type));
}

void listDirectory(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	Client &client = mounts[open->mount].client;
	FileType type = FileType::REGULAR;
	if (!client.readdir(open->backend, message.payload.args.arg2,
						reinterpret_cast<char *>(session.window),
						cock::abi::fs::WINDOW_BYTES, type))
		return reply(pid, id, failure(client));
	reply(pid, id, Status::OK,
		  static_cast<uint32_t>(strlen(reinterpret_cast<char *>(session.window))),
		  static_cast<uint32_t>(type));
}

void closeFile(const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	Client &client = mounts[open->mount].client;
	bool closed = client.close(open->backend);
	Status status = closed ? Status::OK : failure(client);
	memset(open, 0, sizeof(*open));
	reply(pid, id, status);
}

void handle(const Message &message) {
	if (message.sender_pid == cock::ipc::KERNEL_SENDER) return;
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	switch (message.type) {
	case MessageType::FS_CONNECT:
		return connect(pid, id);
	case MessageType::FS_OPEN:
	case MessageType::FS_READ:
	case MessageType::FS_STAT:
	case MessageType::FS_READDIR:
	case MessageType::FS_CLOSE:
		break;
	default:
		return;
	}

	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, pid);
	if (session == nullptr) return reply(pid, id, Status::NOT_CONNECTED);

	switch (message.type) {
	case MessageType::FS_OPEN:
		return openFile(*session, message);
	case MessageType::FS_READ:
		return readFile(*session, message);
	case MessageType::FS_STAT:
		return statFile(message);
	case MessageType::FS_READDIR:
		return listDirectory(*session, message);
	default:
		return closeFile(message);
	}
}

} // namespace

extern "C" void _start() {
	printf("vfs: %u mounts\n", static_cast<unsigned>(MOUNT_COUNT));

	Message message = {};
	for (;;) {
		if (pendingCount > START_COUNT) {
			message = pending[FIRST_PENDING];
			pendingCount--;
			for (size_t index = 0; index < pendingCount; index++)
				pending[index] = pending[index + 1];
		} else {
			cock::user::ipc_recv(message);
		}
		handle(message);
	}
}
