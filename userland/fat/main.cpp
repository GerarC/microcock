#include <cock/abi/block.hpp>
#include <cock/abi/fat32.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/block.hpp>
#include <cock/user/session.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::fat32::Bpb;
using cock::abi::fat32::DirectoryParser;
using cock::abi::fat32::DirEntry;
using cock::abi::fs::FileType;
using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr size_t MAX_CLIENTS = 8;
constexpr size_t MAX_HANDLES = 16;
constexpr size_t MAX_PENDING = 16;
constexpr size_t FIRST_PENDING = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr uint32_t NO_CLUSTER = 0;
constexpr uint32_t NOTHING_LOADED = 0xFFFFFFFF;
constexpr uint32_t FIRST_PARTITION = 1;
constexpr uint32_t SECTOR_ZERO = 0;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr uint32_t NO_BYTES = 0;
constexpr long READ_FAILED = -1;
constexpr char PATH_SEPARATOR = '/';

struct Node {
	uint32_t cluster;
	uint32_t size;
	FileType type;
};

struct Handle {
	bool open;
	Node node;
	uint32_t cursorIndex;
	uint32_t cursorCluster;
};

struct Session {
	bool used;
	uint32_t pid;
	uint32_t windowId;
	uint8_t *window;
	Handle handles[MAX_HANDLES];
};

struct Cursor {
	uint32_t cluster;
	uint32_t sector;
	uint32_t entry;
	uint32_t steps;
};

enum class Link : uint32_t {
	NEXT,
	END,
	BAD,
};

cock::user::block::Client disk;
uint32_t device = 0;
Bpb bpb = {};
Session sessions[MAX_CLIENTS];
Message pending[MAX_PENDING];
size_t pendingCount = 0;
uint8_t fatBuffer[cock::abi::fat32::SECTOR_BYTES];
uint32_t fatLoaded = NOTHING_LOADED;
uint8_t directoryBuffer[cock::abi::fat32::SECTOR_BYTES];
uint32_t directoryLoaded = NOTHING_LOADED;
uint8_t dataBuffer[cock::abi::fat32::SECTOR_BYTES];

void defer(const Message &message) {
	if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
}

Link nextCluster(uint32_t cluster, uint32_t &next) {
	uint32_t sector =
		bpb.reservedSectors + cluster / cock::abi::fat32::FAT_ENTRIES_PER_SECTOR;
	if (sector != fatLoaded) {
		fatLoaded = NOTHING_LOADED;
		if (!disk.read(device, sector, SINGLE_SECTOR, fatBuffer)) return Link::BAD;
		fatLoaded = sector;
	}

	uint32_t value = cock::abi::fat32::fatValue(
		fatBuffer, cluster % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR);
	if (cock::abi::fat32::isEndOfChain(value)) return Link::END;
	if (!bpb.isDataCluster(value)) return Link::BAD;
	next = value;
	return Link::NEXT;
}

bool nextEntry(Cursor &cursor, DirectoryParser &parser, DirEntry &entry) {
	for (;;) {
		if (cursor.entry == cock::abi::fat32::ENTRIES_PER_SECTOR) {
			cursor.entry = 0;
			cursor.sector++;
		}
		if (cursor.sector == bpb.sectorsPerCluster) {
			uint32_t next = NO_CLUSTER;
			if (nextCluster(cursor.cluster, next) != Link::NEXT) return false;
			if (++cursor.steps > bpb.clusterCount) return false;
			cursor.cluster = next;
			cursor.sector = 0;
		}

		uint32_t sector = bpb.clusterSector(cursor.cluster) + cursor.sector;
		if (sector != directoryLoaded) {
			directoryLoaded = NOTHING_LOADED;
			if (!disk.read(device, sector, SINGLE_SECTOR, directoryBuffer)) return false;
			directoryLoaded = sector;
		}

		const uint8_t *raw =
			directoryBuffer + size_t(cursor.entry) * cock::abi::fat32::ENTRY_BYTES;
		cursor.entry++;
		DirectoryParser::Result result = parser.feed(raw, entry);
		if (result == DirectoryParser::Result::END) return false;
		if (result == DirectoryParser::Result::ENTRY) return true;
	}
}

bool findInDirectory(uint32_t directory, const char *name, DirEntry &found) {
	DirectoryParser parser;
	parser.reset();
	Cursor cursor = {directory, 0, 0, 0};
	while (nextEntry(cursor, parser, found))
		if (cock::abi::fat32::namesEqual(found.name, name)) return true;
	return false;
}

bool resolve(const char *path, Node &node) {
	node = {bpb.rootCluster, NO_BYTES, FileType::DIRECTORY};
	while (*path != '\0') {
		while (*path == PATH_SEPARATOR)
			path++;
		if (*path == '\0') break;

		char component[cock::abi::fat32::MAX_NAME_BYTES];
		size_t length = 0;
		while (*path != '\0' && *path != PATH_SEPARATOR) {
			if (length + 1 >= sizeof(component)) return false;
			component[length++] = *path++;
		}
		component[length] = '\0';

		if (node.type != FileType::DIRECTORY) return false;
		DirEntry entry = {};
		if (!findInDirectory(node.cluster, component, entry)) return false;

		if (entry.isDirectory()) {
			if (!bpb.isDataCluster(entry.cluster)) return false;
			node = {entry.cluster, NO_BYTES, FileType::DIRECTORY};
		} else {
			if (entry.size != NO_BYTES && !bpb.isDataCluster(entry.cluster)) return false;
			node = {entry.cluster, entry.size, FileType::REGULAR};
		}
	}
	return true;
}

bool clusterAt(Handle &handle, uint32_t index, uint32_t &cluster) {
	uint32_t at = 0;
	uint32_t current = handle.node.cluster;
	if (handle.cursorCluster != NO_CLUSTER && handle.cursorIndex <= index) {
		at = handle.cursorIndex;
		current = handle.cursorCluster;
	}

	while (at < index) {
		uint32_t next = NO_CLUSTER;
		if (nextCluster(current, next) != Link::NEXT) return false;
		current = next;
		at++;
	}
	handle.cursorIndex = at;
	handle.cursorCluster = current;
	cluster = current;
	return true;
}

long readData(Handle &handle, uint8_t *destination, uint32_t offset, uint32_t count) {
	const uint32_t clusterBytes = bpb.clusterBytes();
	uint32_t done = 0;
	while (done < count) {
		uint32_t position = offset + done;
		uint32_t cluster = NO_CLUSTER;
		if (!clusterAt(handle, position / clusterBytes, cluster)) return READ_FAILED;

		uint32_t within = position % clusterBytes;
		uint32_t sectorInCluster = within / cock::abi::fat32::SECTOR_BYTES;
		uint32_t byteInSector = within % cock::abi::fat32::SECTOR_BYTES;
		uint32_t sector = bpb.clusterSector(cluster) + sectorInCluster;
		uint32_t remaining = count - done;

		if (byteInSector == 0 && remaining >= cock::abi::fat32::SECTOR_BYTES) {
			uint32_t run = remaining / cock::abi::fat32::SECTOR_BYTES;
			if (run > bpb.sectorsPerCluster - sectorInCluster)
				run = bpb.sectorsPerCluster - sectorInCluster;
			if (run > cock::abi::block::MAX_SECTORS_PER_READ)
				run = cock::abi::block::MAX_SECTORS_PER_READ;
			if (!disk.read(device, sector, run, destination + done)) return READ_FAILED;
			done += run * static_cast<uint32_t>(cock::abi::fat32::SECTOR_BYTES);
		} else {
			if (!disk.read(device, sector, SINGLE_SECTOR, dataBuffer)) return READ_FAILED;
			uint32_t take = static_cast<uint32_t>(cock::abi::fat32::SECTOR_BYTES) - byteInSector;
			if (take > remaining) take = remaining;
			memcpy(destination + done, dataBuffer + byteInSector, take);
			done += take;
		}
	}
	return static_cast<long>(done);
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
		memset(session->handles, 0, sizeof(session->handles));
		return reply(pid, id, Status::OK, session->windowId);
	}

	releaseDeadSessions();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr ||
		!cock::user::openSession(*session, pid, cock::abi::fs::WINDOW_PAGES))
		return reply(pid, id, Status::NO_SPACE);
	reply(pid, id, Status::OK, session->windowId);
}

Handle *findHandle(Session &session, uint32_t number) {
	if (number == cock::abi::fs::NO_HANDLE || number > MAX_HANDLES) return nullptr;
	Handle &handle = session.handles[number - 1];
	return handle.open ? &handle : nullptr;
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

	Node node = {};
	if (!resolve(path, node)) return reply(pid, id, Status::NOT_FOUND);

	for (size_t slot = 0; slot < MAX_HANDLES; slot++) {
		if (session.handles[slot].open) continue;
		session.handles[slot] = {true, node, 0, NO_CLUSTER};
		return reply(pid, id, Status::OK, static_cast<uint32_t>(slot + 1));
	}
	reply(pid, id, Status::NO_SPACE);
}

void readFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Handle *handle = findHandle(session, message.payload.args.arg1);
	if (handle == nullptr) return reply(pid, id, Status::BAD_HANDLE);
	if (handle->node.type != FileType::REGULAR) return reply(pid, id, Status::UNSUPPORTED);

	uint32_t offset = message.payload.args.arg2;
	uint32_t length = message.payload.args.arg3;
	if (offset > handle->node.size) return reply(pid, id, Status::OUT_OF_RANGE);

	uint32_t count = handle->node.size - offset;
	if (count > length) count = length;
	if (count > cock::abi::fs::WINDOW_BYTES)
		count = static_cast<uint32_t>(cock::abi::fs::WINDOW_BYTES);
	if (count == 0) return reply(pid, id, Status::OK, NO_BYTES);

	if (readData(*handle, session.window, offset, count) != static_cast<long>(count))
		return reply(pid, id, Status::IO_ERROR);
	reply(pid, id, Status::OK, count);
}

void statFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Handle *handle = findHandle(session, message.payload.args.arg1);
	if (handle == nullptr) return reply(pid, id, Status::BAD_HANDLE);
	reply(pid, id, Status::OK, handle->node.size, static_cast<uint32_t>(handle->node.type));
}

void listDirectory(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Handle *handle = findHandle(session, message.payload.args.arg1);
	if (handle == nullptr) return reply(pid, id, Status::BAD_HANDLE);
	if (handle->node.type != FileType::DIRECTORY)
		return reply(pid, id, Status::UNSUPPORTED);

	uint32_t wanted = message.payload.args.arg2;
	uint32_t seen = 0;
	DirectoryParser parser;
	parser.reset();
	Cursor cursor = {handle->node.cluster, 0, 0, 0};
	DirEntry entry = {};
	while (nextEntry(cursor, parser, entry)) {
		if (seen++ != wanted) continue;

		size_t length = strlen(entry.name);
		memcpy(session.window, entry.name, length + 1);
		return reply(pid, id, Status::OK, static_cast<uint32_t>(length),
					 static_cast<uint32_t>(entry.isDirectory() ? FileType::DIRECTORY
															   : FileType::REGULAR));
	}
	reply(pid, id, Status::OUT_OF_RANGE);
}

void closeFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Handle *handle = findHandle(session, message.payload.args.arg1);
	if (handle == nullptr) return reply(pid, id, Status::BAD_HANDLE);
	handle->open = false;
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
		return statFile(*session, message);
	case MessageType::FS_READDIR:
		return listDirectory(*session, message);
	default:
		return closeFile(*session, message);
	}
}

bool mount() {
	if (!disk.connect()) return false;

	for (uint32_t candidate = FIRST_PARTITION; candidate < disk.deviceCount(); candidate++) {
		cock::user::block::DeviceInfo info = {};
		Bpb found = {};
		if (!disk.info(candidate, info) ||
			!disk.read(candidate, SECTOR_ZERO, SINGLE_SECTOR, dataBuffer))
			continue;
		if (cock::abi::fat32::parseBpb(dataBuffer, info.sectorCount, found) !=
			cock::abi::fat32::BpbError::NONE)
			continue;

		bpb = found;
		device = candidate;
		return true;
	}
	return false;
}

} // namespace

extern "C" void _start() {
	disk.onForeign = defer;
	if (!mount()) {
		printf("fat: no FAT32 volume\n");
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("fat: device %u, %u clusters of %u sectors\n", device, bpb.clusterCount,
		   bpb.sectorsPerCluster);

	Message message = {};
	for (;;) {
		if (pendingCount > 0) {
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
