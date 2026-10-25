#include "file_fuzz.hpp"
#include "prng.hpp"
#include <cock/abi/endpoint.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/names.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

namespace fuzz {

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::Received;
using cock::user::fs::Reply;
using cock::user::fs::Status;

namespace {

constexpr uint32_t REPLY_DEADLINE_MILLISECONDS = 2000;
constexpr uint32_t REPLY_SLICE_MILLISECONDS = 20;
constexpr uint32_t FIRST_REQUEST_ID = 1;
constexpr size_t MAX_KNOWN_HANDLES = 16;
constexpr uint32_t KNOWN_HANDLE_PERCENT = 60;
constexpr uint32_t ACTUAL_LENGTH_PERCENT = 60;
constexpr uint32_t PLAUSIBLE_OFFSET_PERCENT = 50;
constexpr uint32_t PLAUSIBLE_OFFSET_LIMIT = 0x2000;
constexpr uint32_t PLAUSIBLE_FLAGS = 8;
constexpr uint32_t NO_ARGUMENT = 0;
constexpr uint8_t PATH_FILL = 'a';
constexpr uint8_t SEPARATOR_FILL = static_cast<uint8_t>(cock::abi::fs::PATH_SEPARATOR);
constexpr uint32_t BYTE_VALUES = 256;

constexpr const char *PATHS[] = {
	"/",
	"/bin",
	"/bin/",
	"/hello.txt",
	"/bin/hello.elf",
	"/etc",
	"/a",
	"/fat",
	"/fat/fuzz_file",
	"/fat/fuzz_dir",
	"/ext",
	"/ext/fuzz_file",
	"/tmp/x",
	"..",
	"/..",
	"/../..",
	"//",
	"/./.",
	"a/b/c/d/e/f/g/h",
	"/hello.txt/x",
	"/bin/../hello.txt",
	"x",
};

constexpr uint32_t HOSTILE_LENGTHS[] = {
	0x0,
	0x1,
	cock::abi::fs::MAX_PATH_BYTES - 1,
	cock::abi::fs::MAX_PATH_BYTES,
	cock::abi::fs::MAX_PATH_BYTES + 1,
	cock::abi::fs::WINDOW_BYTES - 1,
	cock::abi::fs::WINDOW_BYTES,
	cock::abi::fs::WINDOW_BYTES + 1,
	0x7FFFFFFF,
	0xFFFFFFFF,
};
constexpr uint32_t HOSTILE_HANDLES[] = {0x0, 0x1, 0x2, 0xFF, 0xFFFF, 0x7FFFFFFF, 0xFFFFFFFF};
constexpr uint32_t HOSTILE_OFFSETS[] = {0x0,        0x1,        0xFFF,      0x1000,
										cock::abi::fs::WINDOW_BYTES, 0x7FFFFFFF, 0x80000000,
										0xFFFFF000, 0xFFFFFFFF};
constexpr MessageType REQUESTS[] = {
	MessageType::FS_CONNECT, MessageType::FS_OPEN,   MessageType::FS_OPEN,    MessageType::FS_READ,
	MessageType::FS_WRITE,   MessageType::FS_STAT,   MessageType::FS_CLOSE,   MessageType::FS_READDIR,
	MessageType::FS_UNLINK,  MessageType::FS_MKDIR,  MessageType::FS_RMDIR,   MessageType::FS_RENAME,
	MessageType::FS_LINK,
};

enum class WindowFill : uint32_t {
	PATHS,
	FULL_WINDOW,
	RANDOM_BYTES,
	ZEROS,
	SEPARATORS,
	COUNT,
};

struct PathLengths {
	uint32_t first;
	uint32_t second;
};

struct Session {
	uint32_t pid = cock::user::NO_PROCESS;
	uint32_t route = cock::abi::NO_CAPABILITY_NODE;
	uint8_t *window = nullptr;
	uint32_t object = cock::user::NO_SHM;
	uint32_t nextId = FIRST_REQUEST_ID;
	uint32_t handles[MAX_KNOWN_HANDLES] = {};
	size_t handleCount = 0;
};

bool call(Session &session, MessageType type, uint32_t first, uint32_t second, uint32_t third, Reply &reply) {
	uint32_t id = session.nextId++;
	Message request = {};
	request.type = type;
	request.payload.args.arg0 = id;
	request.payload.args.arg1 = first;
	request.payload.args.arg2 = second;
	request.payload.args.arg3 = third;
	if (!cock::user::ep_send(session.route, request, true)) return false;

	Message answer = {};
	for (uint32_t waited = 0; waited < REPLY_DEADLINE_MILLISECONDS; waited += REPLY_SLICE_MILLISECONDS) {
		Received received = cock::user::ipc_recv_timeout(answer, REPLY_SLICE_MILLISECONDS);
		if (received == Received::FAILED) return false;
		if (received == Received::EMPTY) {
			if (!cock::user::process_alive(session.pid)) return false;
			continue;
		}
		if (answer.type != MessageType::FS_REPLY || answer.badge != cock::abi::endpoint::NO_BADGE ||
			answer.payload.args.arg0 != id)
			continue;

		reply.status = static_cast<Status>(answer.payload.args.arg1);
		reply.first = answer.payload.args.arg2;
		reply.second = answer.payload.args.arg3;
		return true;
	}
	return false;
}

void releaseWindow(Session &session) {
	if (session.window == nullptr) return;
	munmap(session.window, cock::abi::fs::WINDOW_BYTES);
	cock::user::shm_release(session.object);
	session.window = nullptr;
}

bool mapWindow(Session &session, uint32_t object) {
	if (session.window != nullptr && session.object == object) return true;

	uint8_t *mapped = static_cast<uint8_t *>(cock::user::shm_map(object, true));
	releaseWindow(session);
	session.window = mapped;
	session.object = object;
	return mapped != nullptr;
}

bool connect(Session &session, const char *name) {
	cock::user::names::Connection connection = cock::user::names::connect(name);
	if (connection.pid == cock::user::NO_PROCESS || connection.handle == cock::abi::NO_CAPABILITY_NODE)
		return false;

	session.pid = connection.pid;
	session.route = connection.handle;
	Reply reply = {};
	if (!call(session, MessageType::FS_CONNECT, NO_ARGUMENT, NO_ARGUMENT, NO_ARGUMENT, reply) ||
		reply.status != Status::OK)
		return false;
	return mapWindow(session, reply.first);
}

PathLengths fillWindow(Prng &prng, uint8_t *window) {
	WindowFill kind = static_cast<WindowFill>(prng.below(static_cast<uint32_t>(WindowFill::COUNT)));
	PathLengths lengths = {NO_ARGUMENT, NO_ARGUMENT};

	switch (kind) {
	case WindowFill::PATHS: {
		const char *first = prng.pick(PATHS);
		const char *second = prng.pick(PATHS);
		lengths.first = static_cast<uint32_t>(strlen(first));
		lengths.second = static_cast<uint32_t>(strlen(second));
		memcpy(window, first, lengths.first + sizeof(char));
		memcpy(window + cock::abi::fs::RENAME_NEW_OFFSET, second, lengths.second + sizeof(char));
		break;
	}
	case WindowFill::FULL_WINDOW:
		memset(window, PATH_FILL, cock::abi::fs::WINDOW_BYTES);
		lengths.first = cock::abi::fs::MAX_PATH_BYTES - 1;
		lengths.second = cock::abi::fs::MAX_PATH_BYTES - 1;
		break;
	case WindowFill::RANDOM_BYTES:
		for (size_t index = 0; index < cock::abi::fs::WINDOW_BYTES; index++)
			window[index] = static_cast<uint8_t>(prng.below(BYTE_VALUES));
		lengths.first = prng.pick(HOSTILE_LENGTHS);
		lengths.second = prng.pick(HOSTILE_LENGTHS);
		break;
	case WindowFill::ZEROS:
		memset(window, 0, cock::abi::fs::WINDOW_BYTES);
		break;
	case WindowFill::SEPARATORS:
		memset(window, SEPARATOR_FILL, cock::abi::fs::WINDOW_BYTES);
		lengths.first = cock::abi::fs::MAX_PATH_BYTES - 1;
		lengths.second = cock::abi::fs::MAX_PATH_BYTES - 1;
		break;
	case WindowFill::COUNT:
		break;
	}
	return lengths;
}

uint32_t pathLength(Prng &prng, uint32_t actual) {
	return prng.chance(ACTUAL_LENGTH_PERCENT) ? actual : prng.pick(HOSTILE_LENGTHS);
}

uint32_t handleValue(Prng &prng, const Session &session) {
	if (session.handleCount != 0 && prng.chance(KNOWN_HANDLE_PERCENT))
		return session.handles[prng.below(static_cast<uint32_t>(session.handleCount))];
	return prng.chance(KNOWN_HANDLE_PERCENT) ? prng.pick(HOSTILE_HANDLES) : prng.next();
}

uint32_t offsetValue(Prng &prng) {
	return prng.chance(PLAUSIBLE_OFFSET_PERCENT) ? prng.below(PLAUSIBLE_OFFSET_LIMIT) : prng.pick(HOSTILE_OFFSETS);
}

uint32_t flagsValue(Prng &prng) {
	return prng.chance(PLAUSIBLE_OFFSET_PERCENT) ? prng.below(PLAUSIBLE_FLAGS) : prng.next();
}

void rememberHandle(Prng &prng, Session &session, uint32_t handle) {
	if (session.handleCount < MAX_KNOWN_HANDLES) {
		session.handles[session.handleCount++] = handle;
		return;
	}
	session.handles[prng.below(MAX_KNOWN_HANDLES)] = handle;
}

bool sendOne(Prng &prng, Session &session, Reply &reply) {
	MessageType type = prng.pick(REQUESTS);
	PathLengths lengths = fillWindow(prng, session.window);
	uint32_t first = NO_ARGUMENT;
	uint32_t second = NO_ARGUMENT;
	uint32_t third = NO_ARGUMENT;

	switch (type) {
	case MessageType::FS_CONNECT:
		first = prng.chance(PLAUSIBLE_OFFSET_PERCENT) ? prng.below(PLAUSIBLE_FLAGS) : prng.next();
		break;
	case MessageType::FS_OPEN:
		first = pathLength(prng, lengths.first);
		second = flagsValue(prng);
		break;
	case MessageType::FS_READ:
	case MessageType::FS_WRITE:
		first = handleValue(prng, session);
		second = offsetValue(prng);
		third = prng.pick(HOSTILE_LENGTHS);
		break;
	case MessageType::FS_STAT:
	case MessageType::FS_CLOSE:
		first = handleValue(prng, session);
		break;
	case MessageType::FS_READDIR:
		first = handleValue(prng, session);
		second = offsetValue(prng);
		break;
	case MessageType::FS_UNLINK:
	case MessageType::FS_MKDIR:
	case MessageType::FS_RMDIR:
		first = pathLength(prng, lengths.first);
		break;
	default:
		first = pathLength(prng, lengths.first);
		second = pathLength(prng, lengths.second);
		break;
	}

	if (!call(session, type, first, second, third, reply)) return false;
	if (reply.status != Status::OK) return true;

	if (type == MessageType::FS_OPEN) rememberHandle(prng, session, reply.first);
	if (type == MessageType::FS_CONNECT) {
		session.handleCount = 0;
		mapWindow(session, reply.first);
	}
	return true;
}

bool answersValidRequest(Session &session) {
	Reply reply = {};
	return call(session, MessageType::FS_STAT, NO_ARGUMENT, NO_ARGUMENT, NO_ARGUMENT, reply);
}

} // namespace

FileFuzzReport runFileFuzz(const char *serverName, uint32_t seed, uint32_t rounds) {
	FileFuzzReport report = {FileFuzzOutcome::NOT_REACHABLE, 0, 0, 0};
	Session session;
	if (!connect(session, serverName)) return report;

	Prng prng(seed);
	report.outcome = FileFuzzOutcome::SURVIVED;
	for (uint32_t round = 0; round < rounds && session.window != nullptr; round++) {
		Reply reply = {};
		report.requests++;
		if (sendOne(prng, session, reply)) {
			report.answered++;
			continue;
		}
		if (!cock::user::process_alive(session.pid) || !answersValidRequest(session)) {
			report.outcome = FileFuzzOutcome::LOST;
			return report;
		}
		report.timedOut++;
	}

	Session fresh;
	if (!answersValidRequest(session) || !connect(fresh, serverName)) report.outcome = FileFuzzOutcome::LOST;
	releaseWindow(fresh);
	releaseWindow(session);
	return report;
}

} // namespace fuzz
