#include <cock/abi/mounts.hpp>
#include <string.h>
#include <vfs/clients.hpp>
#include <vfs/mounts.hpp>
#include <vfs/opens.hpp>
#include <vfs/reply.hpp>
#include <vfs/requests.hpp>
#include <vfs/routing.hpp>

using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace cock::vfs {

void openFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	uint32_t flags = message.payload.args.arg2;
	if ((flags & ~cock::abi::fs::OPEN_FLAGS_MASK) != 0)
		return reply(pid, id, Status::UNSUPPORTED);

	char full[FULL_PATH_BYTES] = {};
	const char *rest = nullptr;
	size_t index = NOT_FOUND_MOUNT;
	Status routed = route(session, START_OFFSET, message.payload.args.arg1, full, index, rest);
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
	*slot = {true, pid, index, backend, root, false, START_OFFSET};
	reply(pid, id, Status::OK, static_cast<uint32_t>(slot - opens) + 1);
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

void statFile(const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;
	Open *open = findOpen(pid, message.payload.args.arg1);
	if (open == nullptr) return reply(pid, id, Status::BAD_HANDLE);

	size_t index = open->mount;
	FileInfo info = {};
	if (!mounts[index].client.stat(open->backend, info)) return reply(pid, id, failure(index));
	reply(pid, id, Status::OK, info.size, static_cast<uint32_t>(info.type));
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

} // namespace cock::vfs
