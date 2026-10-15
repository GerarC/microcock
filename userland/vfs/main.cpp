#include <cock/abi/fs.hpp>
#include <cock/abi/mounts.hpp>
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
constexpr uint32_t START_OFFSET = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr long MORE_TO_READ = 1;

struct Mount {
	char prefix[cock::abi::mounts::MAX_PREFIX_BYTES];
	char server[cock::abi::mounts::MAX_SERVER_BYTES];
	size_t prefixLength;
	Client client;
	bool connected;
};

struct Open {
	bool used;
	uint32_t owner;
	size_t mount;
	uint32_t backend;
	bool root;
	bool countKnown;
	uint32_t backendEntries;
};

struct Session {
	bool used;
	uint32_t pid;
	uint32_t windowId;
	uint8_t *window;
};

Mount mounts[cock::abi::mounts::MAX_MOUNTS];
size_t mountCount = 0;
Session sessions[MAX_CLIENTS];
Open opens[MAX_OPEN];
Message pending[MAX_PENDING];
size_t pendingCount = 0;
char mountsText[cock::abi::mounts::MAX_FILE_BYTES];
cock::abi::mounts::Table table;

void defer(const Message &message) {
	if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
}

bool readInto(Client &client, uint32_t handle, char *destination, uint32_t size) {
	uint32_t copied = START_OFFSET;
	long step = MORE_TO_READ;
	while (copied < size && step > 0) {
		step = client.read(handle, copied, destination + copied, size - copied);
		if (step > 0) copied += static_cast<uint32_t>(step);
	}
	return copied == size;
}

bool loadMounts() {
	Client client;
	client.onForeign = defer;
	if (!client.connect(cock::abi::fs::RAMFS_SERVER_NAME)) return false;

	uint32_t handle = client.open(cock::abi::mounts::FILE_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool loaded = client.stat(handle, info) && info.size > START_OFFSET &&
				  info.size <= sizeof(mountsText) &&
				  readInto(client, handle, mountsText, info.size);
	client.close(handle);
	if (!loaded) return false;

	if (!table.parse(mountsText, info.size)) {
		printf("vfs: %s line %u rejected, error %u\n", cock::abi::mounts::FILE_PATH,
			   static_cast<unsigned>(table.errorLine),
			   static_cast<unsigned>(table.error));
		return false;
	}

	for (size_t index = 0; index < table.count; index++) {
		Mount &mount = mounts[index];
		memcpy(mount.prefix, table.mounts[index].prefix, sizeof(mount.prefix));
		memcpy(mount.server, table.mounts[index].server, sizeof(mount.server));
		mount.prefixLength = strlen(mount.prefix);
	}
	mountCount = table.count;
	return true;
}

size_t pickMount(const char *full, const char *&rest) {
	size_t best = NOT_FOUND_MOUNT;
	size_t bestLength = 0;
	size_t fullLength = strlen(full);
	for (size_t index = 0; index < mountCount; index++) {
		size_t length = mounts[index].prefixLength;
		if (length < bestLength || fullLength < length) continue;
		if (memcmp(full, mounts[index].prefix, length) != 0) continue;

		bool whole = length == SEPARATOR_BYTES || full[length] == '\0' ||
					 full[length] == PATH_SEPARATOR;
		if (!whole) continue;
		best = index;
		bestLength = length;
	}

	if (best == NOT_FOUND_MOUNT) return best;
	rest = bestLength == SEPARATOR_BYTES ? full : full + bestLength;
	if (*rest == '\0') rest = cock::abi::mounts::ROOT_PREFIX;
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

void markDown(size_t index) {
	mounts[index].connected = false;
	for (Open &open : opens)
		if (open.used && open.mount == index) memset(&open, 0, sizeof(open));
}

Status failure(size_t index) {
	Status status = mounts[index].client.lastStatus();
	if (status == Status::NOT_CONNECTED) markDown(index);
	return status == Status::OK ? Status::IO_ERROR : status;
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

Status route(const Session &session, size_t at, uint32_t length, char *full, size_t &index,
			 const char *&rest) {
	if (length == 0 || length >= cock::abi::fs::MAX_PATH_BYTES) return Status::BAD_REQUEST;

	char path[cock::abi::fs::MAX_PATH_BYTES] = {};
	memcpy(path, session.window + at, length);
	if (strlen(path) != length) return Status::BAD_REQUEST;

	size_t offset = 0;
	if (path[0] != PATH_SEPARATOR) full[offset++] = PATH_SEPARATOR;
	memcpy(full + offset, path, length);

	index = pickMount(full, rest);
	if (index == NOT_FOUND_MOUNT || strlen(rest) >= cock::abi::fs::MAX_PATH_BYTES)
		return Status::BAD_REQUEST;
	return ensureMount(index) ? Status::OK : Status::NOT_FOUND;
}

void openFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	uint32_t flags = message.payload.args.arg2;
	if ((flags & ~cock::abi::fs::OPEN_FLAGS_MASK) != 0)
		return reply(pid, id, Status::UNSUPPORTED);

	char full[cock::abi::fs::MAX_PATH_BYTES + SEPARATOR_BYTES] = {};
	const char *rest = nullptr;
	size_t index = NOT_FOUND_MOUNT;
	Status routed = route(session, 0, message.payload.args.arg1, full, index, rest);
	if (routed != Status::OK) return reply(pid, id, routed);

	Open *slot = freeOpen();
	if (slot == nullptr) {
		releaseDead();
		slot = freeOpen();
	}
	if (slot == nullptr) return reply(pid, id, Status::NO_SPACE);

	Client &client = mounts[index].client;
	uint32_t backend = client.open(rest, flags);
	if (backend == cock::abi::fs::NO_HANDLE) return reply(pid, id, failure(index));

	bool root = strcmp(full, cock::abi::mounts::ROOT_PREFIX) == 0;
	*slot = {true, pid, index, backend, root, false, 0};
	reply(pid, id, Status::OK, static_cast<uint32_t>(slot - opens) + 1);
}

void writeFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	size_t index = open->mount;
	long count = mounts[index].client.write(open->backend, message.payload.args.arg2,
											session.window, message.payload.args.arg3);
	if (count < 0) return reply(pid, id, failure(index));
	reply(pid, id, Status::OK, static_cast<uint32_t>(count));
}

void pathRequest(Session &session, const Message &message,
				 bool (Client::*operation)(const char *)) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	char full[cock::abi::fs::MAX_PATH_BYTES + SEPARATOR_BYTES] = {};
	const char *rest = nullptr;
	size_t index = NOT_FOUND_MOUNT;
	Status routed = route(session, 0, message.payload.args.arg1, full, index, rest);
	if (routed != Status::OK) return reply(pid, id, routed);

	if (!(mounts[index].client.*operation)(rest)) return reply(pid, id, failure(index));
	reply(pid, id, Status::OK);
}

void renameEntry(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	char fullFrom[cock::abi::fs::MAX_PATH_BYTES + SEPARATOR_BYTES] = {};
	char fullTo[cock::abi::fs::MAX_PATH_BYTES + SEPARATOR_BYTES] = {};
	const char *restFrom = nullptr;
	const char *restTo = nullptr;
	size_t fromIndex = NOT_FOUND_MOUNT;
	size_t toIndex = NOT_FOUND_MOUNT;
	Status routed =
		route(session, 0, message.payload.args.arg1, fullFrom, fromIndex, restFrom);
	if (routed != Status::OK) return reply(pid, id, routed);
	routed = route(session, cock::abi::fs::RENAME_NEW_OFFSET, message.payload.args.arg2, fullTo,
				   toIndex, restTo);
	if (routed != Status::OK) return reply(pid, id, routed);
	if (fromIndex != toIndex) return reply(pid, id, Status::CROSS_DEVICE);

	if (!mounts[fromIndex].client.rename(restFrom, restTo))
		return reply(pid, id, failure(fromIndex));
	reply(pid, id, Status::OK);
}

void unlinkFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	char full[cock::abi::fs::MAX_PATH_BYTES + SEPARATOR_BYTES] = {};
	const char *rest = nullptr;
	size_t index = NOT_FOUND_MOUNT;
	Status routed = route(session, 0, message.payload.args.arg1, full, index, rest);
	if (routed != Status::OK) return reply(pid, id, routed);

	if (!mounts[index].client.unlink(rest)) return reply(pid, id, failure(index));
	reply(pid, id, Status::OK);
}

void readFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	size_t index = open->mount;
	long count = mounts[index].client.read(open->backend, message.payload.args.arg2,
										   session.window, message.payload.args.arg3);
	if (count < 0) return reply(pid, id, failure(index));
	reply(pid, id, Status::OK, static_cast<uint32_t>(count));
}

void statFile(const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	size_t index = open->mount;
	FileInfo info = {};
	if (!mounts[index].client.stat(open->backend, info))
		return reply(pid, id, failure(index));
	reply(pid, id, Status::OK, info.size, static_cast<uint32_t>(info.type));
}

bool isMountPoint(const Mount &mount, const char *&name) {
	if (mount.prefixLength <= SEPARATOR_BYTES) return false;
	name = mount.prefix + SEPARATOR_BYTES;
	for (const char *cursor = name; *cursor != '\0'; cursor++)
		if (*cursor == PATH_SEPARATOR) return false;
	return true;
}

uint32_t countEntries(Open &open, Session &session) {
	if (open.countKnown) return open.backendEntries;

	FileType type = FileType::REGULAR;
	uint32_t count = START_OFFSET;
	Client &client = mounts[open.mount].client;
	while (client.readdir(open.backend, count, reinterpret_cast<char *>(session.window),
						  cock::abi::fs::WINDOW_BYTES, type))
		count++;
	open.countKnown = true;
	open.backendEntries = count;
	return count;
}

bool replyMountPoint(uint32_t pid, uint32_t id, Session &session, uint32_t wanted) {
	uint32_t seen = START_OFFSET;
	for (size_t index = 0; index < mountCount; index++) {
		const char *name = nullptr;
		if (!isMountPoint(mounts[index], name)) continue;
		if (seen++ != wanted) continue;

		size_t length = strlen(name);
		memcpy(session.window, name, length + SEPARATOR_BYTES);
		reply(pid, id, Status::OK, static_cast<uint32_t>(length),
			  static_cast<uint32_t>(FileType::DIRECTORY));
		return true;
	}
	return false;
}

void listDirectory(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	size_t index = open->mount;
	uint32_t wanted = message.payload.args.arg2;
	Client &client = mounts[index].client;
	FileType type = FileType::REGULAR;
	if (client.readdir(open->backend, wanted, reinterpret_cast<char *>(session.window),
					   cock::abi::fs::WINDOW_BYTES, type))
		return reply(pid, id, Status::OK,
					 static_cast<uint32_t>(strlen(reinterpret_cast<char *>(session.window))),
					 static_cast<uint32_t>(type));

	if (!open->root || client.lastStatus() != Status::OUT_OF_RANGE)
		return reply(pid, id, failure(index));

	uint32_t backendEntries = countEntries(*open, session);
	if (wanted < backendEntries || !replyMountPoint(pid, id, session, wanted - backendEntries))
		return reply(pid, id, Status::OUT_OF_RANGE);
}

void closeFile(const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	size_t index = open->mount;
	bool closed = mounts[index].client.close(open->backend);
	Status status = closed ? Status::OK : failure(index);
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
	case MessageType::FS_WRITE:
	case MessageType::FS_UNLINK:
	case MessageType::FS_MKDIR:
	case MessageType::FS_RMDIR:
	case MessageType::FS_RENAME:
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
	case MessageType::FS_WRITE:
		return writeFile(*session, message);
	case MessageType::FS_UNLINK:
		return unlinkFile(*session, message);
	case MessageType::FS_MKDIR:
		return pathRequest(*session, message, &Client::mkdir);
	case MessageType::FS_RMDIR:
		return pathRequest(*session, message, &Client::rmdir);
	case MessageType::FS_RENAME:
		return renameEntry(*session, message);
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
	if (!loadMounts()) {
		printf("vfs: cannot read %s\n", cock::abi::mounts::FILE_PATH);
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("vfs: %u mounts\n", static_cast<unsigned>(mountCount));

	Message message = {};
	for (;;) {
		if (pendingCount > START_OFFSET) {
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
