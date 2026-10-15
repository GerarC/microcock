#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/session.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

using cock::abi::fs::FileType;
using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_SHM;

namespace {

constexpr size_t TAR_BLOCK_BYTES = 512;
constexpr size_t TAR_NAME_OFFSET = 0;
constexpr size_t TAR_NAME_BYTES = 100;
constexpr size_t TAR_SIZE_OFFSET = 124;
constexpr size_t TAR_SIZE_BYTES = 12;
constexpr size_t TAR_TYPE_OFFSET = 156;
constexpr size_t TAR_MAGIC_OFFSET = 257;
constexpr size_t TAR_MAGIC_BYTES = 5;
constexpr char TAR_MAGIC[] = "ustar";
constexpr char OCTAL_FIRST_DIGIT = '0';
constexpr char TAR_REGULAR = OCTAL_FIRST_DIGIT;
constexpr char TAR_REGULAR_LEGACY = '\0';
constexpr char TAR_DIRECTORY = '5';
constexpr char PATH_SEPARATOR = '/';
constexpr uint32_t OCTAL_BASE = 8;
constexpr uint32_t MAX_ARCHIVE_BYTES = 0x80000;

constexpr size_t ROOT_ENTRY = 0;
constexpr size_t FIRST_FILE_ENTRY = 1;
constexpr size_t MAX_ENTRIES = 64;
constexpr size_t MAX_CLIENTS = 8;
constexpr size_t MAX_HANDLES = 16;
constexpr size_t MAX_PENDING = 16;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;

struct Entry {
	char name[TAR_NAME_BYTES + 1];
	uint32_t offset;
	uint32_t size;
	FileType type;
};

struct Session {
	bool used;
	uint32_t pid;
	uint32_t windowId;
	uint8_t *window;
	bool open[MAX_HANDLES];
	uint32_t entry[MAX_HANDLES];
};

const uint8_t *archive = nullptr;
Entry entries[MAX_ENTRIES];
size_t entryCount = FIRST_FILE_ENTRY;
Session sessions[MAX_CLIENTS];
Message pending[MAX_PENDING];
size_t pendingCount = 0;

bool parseOctal(const uint8_t *field, size_t length, uint32_t &value) {
	value = 0;
	for (size_t idx = 0; idx < length && field[idx] != '\0' && field[idx] != ' ';
		 idx++) {
		if (field[idx] < OCTAL_FIRST_DIGIT ||
			field[idx] >= OCTAL_FIRST_DIGIT + OCTAL_BASE)
			return false;
		value = value * OCTAL_BASE +
				static_cast<uint32_t>(field[idx] - OCTAL_FIRST_DIGIT);
		if (value > MAX_ARCHIVE_BYTES) return false;
	}
	return true;
}

void buildIndex() {
	entries[ROOT_ENTRY].type = FileType::DIRECTORY;
	uint32_t position = 0;
	while (entryCount < MAX_ENTRIES &&
		   position + TAR_BLOCK_BYTES <= MAX_ARCHIVE_BYTES) {
		const uint8_t *header = archive + position;
		if (header[TAR_NAME_OFFSET] == TAR_REGULAR_LEGACY) break;
		if (memcmp(header + TAR_MAGIC_OFFSET, TAR_MAGIC, TAR_MAGIC_BYTES) != 0) break;

		uint32_t size = 0;
		if (!parseOctal(header + TAR_SIZE_OFFSET, TAR_SIZE_BYTES, size)) break;

		char type = static_cast<char>(header[TAR_TYPE_OFFSET]);
		bool regular = type == TAR_REGULAR || type == TAR_REGULAR_LEGACY;
		if (regular || type == TAR_DIRECTORY) {
			Entry &entry = entries[entryCount++];
			memset(&entry, 0, sizeof(entry));
			memcpy(entry.name, header, TAR_NAME_BYTES);
			size_t length = strlen(entry.name);
			while (length > 0 && entry.name[length - 1] == PATH_SEPARATOR)
				entry.name[--length] = '\0';
			entry.offset = position + TAR_BLOCK_BYTES;
			entry.size = regular ? size : 0;
			entry.type = regular ? FileType::REGULAR : FileType::DIRECTORY;
		}

		uint32_t blocks = (size + TAR_BLOCK_BYTES - 1) / TAR_BLOCK_BYTES;
		position += TAR_BLOCK_BYTES + blocks * TAR_BLOCK_BYTES;
	}
}

void releaseDeadSessions() {
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
		memset(session->open, 0, sizeof(session->open));
		reply(pid, id, Status::OK, session->windowId);
		return;
	}

	releaseDeadSessions();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr || !cock::user::openSession(*session, pid, cock::abi::fs::WINDOW_PAGES))
		return reply(pid, id, Status::NO_SPACE);
	reply(pid, id, Status::OK, session->windowId);
}

const Entry *findEntry(const char *path) {
	if (*path == PATH_SEPARATOR) path++;
	size_t length = strlen(path);
	while (length > 0 && path[length - 1] == PATH_SEPARATOR)
		length--;
	if (length > TAR_NAME_BYTES) return nullptr;
	if (length == 0) return &entries[ROOT_ENTRY];

	for (size_t idx = FIRST_FILE_ENTRY; idx < entryCount; idx++) {
		if (strlen(entries[idx].name) == length &&
			memcmp(entries[idx].name, path, length) == 0)
			return &entries[idx];
	}
	return nullptr;
}

void openFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	uint32_t length = message.payload.args.arg1;
	uint32_t flags = message.payload.args.arg2;
	if ((flags & ~cock::abi::fs::OPEN_FLAGS_MASK) != 0)
		return reply(pid, id, Status::UNSUPPORTED);
	if (flags != cock::abi::fs::OPEN_READ_ONLY) return reply(pid, id, Status::READ_ONLY);
	if (length == 0 || length >= cock::abi::fs::MAX_PATH_BYTES)
		return reply(pid, id, Status::BAD_REQUEST);

	char path[cock::abi::fs::MAX_PATH_BYTES] = {};
	memcpy(path, session.window, length);
	if (strlen(path) != length) return reply(pid, id, Status::BAD_REQUEST);

	const Entry *entry = findEntry(path);
	if (entry == nullptr) return reply(pid, id, Status::NOT_FOUND);

	for (size_t slot = 0; slot < MAX_HANDLES; slot++) {
		if (session.open[slot]) continue;
		session.open[slot] = true;
		session.entry[slot] = static_cast<uint32_t>(entry - entries);
		return reply(pid, id, Status::OK, static_cast<uint32_t>(slot + 1));
	}
	reply(pid, id, Status::NO_SPACE);
}

const Entry *handleEntry(const Session &session, uint32_t handle) {
	if (handle == cock::abi::fs::NO_HANDLE || handle > MAX_HANDLES) return nullptr;
	size_t slot = handle - 1;
	return session.open[slot] ? &entries[session.entry[slot]] : nullptr;
}

void readFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	const Entry *entry = handleEntry(session, message.payload.args.arg1);
	if (entry == nullptr) return reply(pid, id, Status::BAD_HANDLE);
	if (entry->type != FileType::REGULAR) return reply(pid, id, Status::UNSUPPORTED);

	uint32_t offset = message.payload.args.arg2;
	uint32_t length = message.payload.args.arg3;
	if (offset > entry->size) return reply(pid, id, Status::OUT_OF_RANGE);

	uint32_t count = entry->size - offset;
	if (count > length) count = length;
	if (count > cock::abi::fs::WINDOW_BYTES)
		count = static_cast<uint32_t>(cock::abi::fs::WINDOW_BYTES);

	memcpy(session.window, archive + entry->offset + offset, count);
	reply(pid, id, Status::OK, count);
}

void statFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	const Entry *entry = handleEntry(session, message.payload.args.arg1);
	if (entry == nullptr) return reply(pid, id, Status::BAD_HANDLE);
	reply(pid, id, Status::OK, entry->size, static_cast<uint32_t>(entry->type));
}

bool isChild(const Entry &directory, const Entry &candidate, const char *&name) {
	name = candidate.name;
	size_t prefix = strlen(directory.name);
	if (prefix > 0) {
		if (strlen(name) <= prefix + 1 || memcmp(name, directory.name, prefix) != 0 ||
			name[prefix] != PATH_SEPARATOR)
			return false;
		name += prefix + 1;
	}
	for (const char *cursor = name; *cursor != '\0'; cursor++)
		if (*cursor == PATH_SEPARATOR) return false;
	return true;
}

void listDirectory(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	const Entry *directory = handleEntry(session, message.payload.args.arg1);
	if (directory == nullptr) return reply(pid, id, Status::BAD_HANDLE);
	if (directory->type != FileType::DIRECTORY)
		return reply(pid, id, Status::UNSUPPORTED);

	uint32_t wanted = message.payload.args.arg2;
	uint32_t seen = 0;
	for (size_t idx = FIRST_FILE_ENTRY; idx < entryCount; idx++) {
		const char *name = nullptr;
		if (!isChild(*directory, entries[idx], name)) continue;
		if (seen++ != wanted) continue;

		size_t length = strlen(name);
		memcpy(session.window, name, length + 1);
		return reply(pid, id, Status::OK, static_cast<uint32_t>(length),
					 static_cast<uint32_t>(entries[idx].type));
	}
	reply(pid, id, Status::OUT_OF_RANGE);
}

void closeFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	uint32_t handle = message.payload.args.arg1;
	if (handleEntry(session, handle) == nullptr)
		return reply(pid, id, Status::BAD_HANDLE);
	session.open[handle - 1] = false;
	reply(pid, id, Status::OK);
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
	case MessageType::FS_UNLINK:
		return reply(pid, id, Status::READ_ONLY);
	case MessageType::FS_STAT:
		return statFile(*session, message);
	case MessageType::FS_READDIR:
		return listDirectory(*session, message);
	default:
		return closeFile(*session, message);
	}
}

} // namespace

extern "C" void _start() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) break;
		if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
	}

	uint32_t module = cock::user::module_open(cock::abi::fs::INITRD_MODULE_NAME);
	if (module == NO_SHM) cock::user::exit_process(EXIT_FAILED_STATUS);

	archive = static_cast<const uint8_t *>(cock::user::shm_map(module, false));
	if (archive == nullptr) cock::user::exit_process(EXIT_FAILED_STATUS);
	buildIndex();

	for (size_t idx = 0; idx < pendingCount; idx++)
		handle(pending[idx]);

	for (;;) {
		cock::user::ipc_recv(message);
		handle(message);
	}
}
