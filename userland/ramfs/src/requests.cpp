#include <cock/abi/fs.hpp>
#include <ramfs/archive.hpp>
#include <ramfs/reply.hpp>
#include <ramfs/requests.hpp>
#include <ramfs/sessions.hpp>
#include <string.h>

using cock::abi::fs::FileType;
using cock::abi::fs::Status;
using cock::ipc::Message;

namespace cock::ramfs {

namespace {

constexpr uint32_t NO_ENTRIES_SEEN = 0;
constexpr size_t TERMINATOR_BYTES = 1;

} // namespace

void openFile(Session &session, const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	uint32_t length = message.payload.args.arg1;
	uint32_t flags = message.payload.args.arg2;
	if ((flags & ~cock::abi::fs::OPEN_FLAGS_MASK) != 0)
		return reply(token, id, Status::UNSUPPORTED);
	if (flags != cock::abi::fs::OPEN_READ_ONLY) return reply(token, id, Status::READ_ONLY);
	if (length == 0 || length >= cock::abi::fs::MAX_PATH_BYTES)
		return reply(token, id, Status::BAD_REQUEST);

	char path[cock::abi::fs::MAX_PATH_BYTES] = {};
	memcpy(path, session.window, length);
	if (strlen(path) != length) return reply(token, id, Status::BAD_REQUEST);

	const Entry *entry = findEntry(path);
	if (entry == nullptr) return reply(token, id, Status::NOT_FOUND);

	for (size_t slot = 0; slot < MAX_HANDLES; slot++) {
		if (session.open[slot]) continue;
		session.open[slot] = true;
		session.entry[slot] = static_cast<uint32_t>(entry - entries);
		return reply(token, id, Status::OK, static_cast<uint32_t>(slot + 1));
	}
	reply(token, id, Status::NO_SPACE);
}

void readFile(Session &session, const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	const Entry *entry = handleEntry(session, message.payload.args.arg1);
	if (entry == nullptr) return reply(token, id, Status::BAD_HANDLE);
	if (entry->type != FileType::REGULAR) return reply(token, id, Status::UNSUPPORTED);

	uint32_t offset = message.payload.args.arg2;
	uint32_t length = message.payload.args.arg3;
	if (offset > entry->size) return reply(token, id, Status::OUT_OF_RANGE);

	uint32_t count = entry->size - offset;
	if (count > length) count = length;
	if (count > cock::abi::fs::WINDOW_BYTES)
		count = static_cast<uint32_t>(cock::abi::fs::WINDOW_BYTES);

	memcpy(session.window, archive + entry->offset + offset, count);
	reply(token, id, Status::OK, count);
}

void statFile(Session &session, const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	const Entry *entry = handleEntry(session, message.payload.args.arg1);
	if (entry == nullptr) return reply(token, id, Status::BAD_HANDLE);
	reply(token, id, Status::OK, entry->size, static_cast<uint32_t>(entry->type));
}

void listDirectory(Session &session, const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	const Entry *directory = handleEntry(session, message.payload.args.arg1);
	if (directory == nullptr) return reply(token, id, Status::BAD_HANDLE);
	if (directory->type != FileType::DIRECTORY) return reply(token, id, Status::UNSUPPORTED);

	uint32_t wanted = message.payload.args.arg2;
	uint32_t seen = NO_ENTRIES_SEEN;
	for (size_t idx = FIRST_FILE_ENTRY; idx < entryCount; idx++) {
		const char *name = nullptr;
		if (!isChild(*directory, entries[idx], name)) continue;
		if (seen++ != wanted) continue;

		size_t length = strlen(name);
		memcpy(session.window, name, length + TERMINATOR_BYTES);
		return reply(token, id, Status::OK, static_cast<uint32_t>(length),
					 static_cast<uint32_t>(entries[idx].type));
	}
	reply(token, id, Status::OUT_OF_RANGE);
}

void closeFile(Session &session, const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	uint32_t handle = message.payload.args.arg1;
	if (handleEntry(session, handle) == nullptr) return reply(token, id, Status::BAD_HANDLE);
	session.open[handle - 1] = false;
	reply(token, id, Status::OK);
}

} // namespace cock::ramfs
