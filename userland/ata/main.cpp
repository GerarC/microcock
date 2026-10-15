#include "ata.hpp"
#include <cock/abi/block.hpp>
#include <cock/abi/partition.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

using cock::abi::block::DeviceKind;
using cock::abi::block::Status;
using cock::abi::partition::GptHeader;
using cock::abi::partition::MbrResult;
using cock::abi::partition::Scheme;
using cock::abi::partition::Table;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_SHM;

namespace {

constexpr size_t MAX_CLIENTS = 8;
constexpr size_t MAX_PENDING = 16;
constexpr uint32_t DISK_FIRST_SECTOR = 0;
constexpr uint32_t MBR_SECTOR = 0;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr uint32_t GPT_HEADER_SECTOR = cock::abi::partition::GPT_HEADER_SECTOR;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;

struct Device {
	uint32_t firstSector;
	uint32_t sectorCount;
	DeviceKind kind;
};

struct Session {
	bool used;
	uint32_t pid;
	uint32_t windowId;
	uint8_t *window;
};

ata::Drive drive;
Device devices[cock::abi::block::MAX_DEVICES];
uint32_t deviceCount = 0;
Session sessions[MAX_CLIENTS];
Message pending[MAX_PENDING];
size_t pendingCount = 0;
uint8_t sectorBuffer[cock::abi::block::SECTOR_BYTES];
uint8_t entriesBuffer[cock::abi::partition::GPT_ENTRIES_MAX_BYTES];

void addDevice(uint32_t first, uint32_t count, DeviceKind kind) {
	devices[deviceCount++] = {first, count, kind};
}

void addPartitions(const Table &table, DeviceKind kind) {
	for (size_t slot = 0; slot < table.count; slot++)
		addDevice(static_cast<uint32_t>(table.entries[slot].firstSector),
				  static_cast<uint32_t>(table.entries[slot].sectorCount), kind);
}

bool readGptEntries(const GptHeader &header, uint32_t diskSectors, Table &table) {
	size_t sectors = header.entriesSectors();
	for (size_t index = 0; index < sectors; index++) {
		if (!drive.read(static_cast<uint32_t>(header.entriesSector + index), SINGLE_SECTOR,
						entriesBuffer + index * cock::abi::block::SECTOR_BYTES))
			return false;
	}
	return cock::abi::partition::parseGptEntries(entriesBuffer, header, diskSectors,
												 table);
}

void scanPartitions() {
	addDevice(DISK_FIRST_SECTOR, drive.sectors(), DeviceKind::DISK);
	uint32_t diskSectors = drive.sectors();

	Table table = {};
	if (!drive.read(MBR_SECTOR, SINGLE_SECTOR, sectorBuffer)) return;

	MbrResult result = cock::abi::partition::parseMbr(sectorBuffer, diskSectors, table);
	if (result == MbrResult::PRIMARY) return addPartitions(table, DeviceKind::MBR_PARTITION);
	if (result != MbrResult::PROTECTIVE) return;

	GptHeader header = {};
	if (!drive.read(GPT_HEADER_SECTOR, SINGLE_SECTOR, sectorBuffer) ||
		!cock::abi::partition::parseGptHeader(sectorBuffer, diskSectors, header) ||
		!readGptEntries(header, diskSectors, table)) {
		printf("ata: GPT rejected\n");
		return;
	}
	addPartitions(table, DeviceKind::GPT_PARTITION);
}

Session *findSession(uint32_t pid) {
	for (Session &session : sessions)
		if (session.used && session.pid == pid) return &session;
	return nullptr;
}

Session *freeSession() {
	for (Session &session : sessions)
		if (!session.used) return &session;
	return nullptr;
}

void releaseSession(Session &session) {
	munmap(session.window, cock::abi::block::WINDOW_BYTES);
	cock::user::shm_release(session.windowId);
	memset(&session, 0, sizeof(session));
}

void releaseDeadSessions() {
	for (Session &session : sessions)
		if (session.used && !cock::user::process_alive(session.pid))
			releaseSession(session);
}

void reply(uint32_t pid, uint32_t id, Status status, uint32_t first = 0,
		   uint32_t second = 0, uint32_t third = 0) {
	Message answer = {};
	answer.type = MessageType::BLK_REPLY;
	answer.payload.args.arg0 = id;
	answer.payload.args.arg1 = static_cast<uint32_t>(status);
	answer.payload.args.arg2 = first;
	answer.payload.args.arg3 = second;
	answer.payload.args.arg4 = third;
	cock::user::ipc_send(pid, answer);
}

void connect(uint32_t pid, uint32_t id) {
	Session *session = findSession(pid);
	if (session != nullptr) return reply(pid, id, Status::OK, session->windowId, deviceCount);

	releaseDeadSessions();
	session = freeSession();
	if (session == nullptr) return reply(pid, id, Status::NO_SPACE);

	uint32_t window = cock::user::shm_create(cock::abi::block::WINDOW_PAGES);
	if (window == NO_SHM) return reply(pid, id, Status::NO_SPACE);

	uint8_t *view = nullptr;
	if (cock::user::shm_share(window, pid, true))
		view = static_cast<uint8_t *>(cock::user::shm_map(window, true));
	if (view == nullptr) {
		cock::user::shm_release(window);
		return reply(pid, id, Status::NO_SPACE);
	}

	memset(session, 0, sizeof(*session));
	session->used = true;
	session->pid = pid;
	session->windowId = window;
	session->window = view;
	reply(pid, id, Status::OK, window, deviceCount);
}

void answerInfo(const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	uint32_t device = message.payload.args.arg1;
	if (device >= deviceCount) return reply(pid, id, Status::BAD_DEVICE);

	const Device &found = devices[device];
	reply(pid, id, Status::OK, found.sectorCount, found.firstSector,
		  static_cast<uint32_t>(found.kind));
}

void answerRead(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	uint32_t device = message.payload.args.arg1;
	uint32_t sector = message.payload.args.arg2;
	uint32_t count = message.payload.args.arg3;
	if (device >= deviceCount) return reply(pid, id, Status::BAD_DEVICE);
	if (count == 0 || count > cock::abi::block::MAX_SECTORS_PER_READ)
		return reply(pid, id, Status::BAD_REQUEST);

	const Device &found = devices[device];
	if (sector >= found.sectorCount || count > found.sectorCount - sector)
		return reply(pid, id, Status::OUT_OF_RANGE);

	if (!drive.read(found.firstSector + sector, count, session.window))
		return reply(pid, id, Status::IO_ERROR);
	reply(pid, id, Status::OK, count);
}

void handle(const Message &message) {
	if (message.sender_pid == cock::ipc::KERNEL_SENDER) return;
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	switch (message.type) {
	case MessageType::BLK_CONNECT:
		return connect(pid, id);
	case MessageType::BLK_INFO:
	case MessageType::BLK_READ:
		break;
	default:
		return;
	}

	Session *session = findSession(pid);
	if (session == nullptr) return reply(pid, id, Status::NOT_CONNECTED);

	if (message.type == MessageType::BLK_INFO) return answerInfo(message);
	answerRead(*session, message);
}

} // namespace

extern "C" void _start() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) break;
		if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
	}

	if (!drive.init()) {
		printf("ata: no drive\n");
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}

	scanPartitions();
	printf("ata: %u sectors, %u devices\n", drive.sectors(), deviceCount);

	for (size_t index = 0; index < pendingCount; index++)
		handle(pending[index]);

	for (;;) {
		cock::user::ipc_recv(message);
		handle(message);
	}
}
