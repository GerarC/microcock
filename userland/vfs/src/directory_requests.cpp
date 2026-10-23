#include <string.h>
#include <vfs/clients.hpp>
#include <vfs/mounts.hpp>
#include <vfs/opens.hpp>
#include <vfs/reply.hpp>
#include <vfs/requests.hpp>

using cock::abi::fs::FileType;
using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::user::fs::Client;

namespace cock::vfs {

namespace {

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

} // namespace

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

} // namespace cock::vfs
