#include <vfs/clients.hpp>
#include <vfs/mounts.hpp>
#include <vfs/reply.hpp>
#include <vfs/requests.hpp>
#include <vfs/routing.hpp>

using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::user::fs::Client;

namespace cock::vfs {

void pathRequest(Session &session, const Message &message,
				 bool (Client::*operation)(const char *)) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	char full[FULL_PATH_BYTES] = {};
	const char *rest = nullptr;
	size_t index = NOT_FOUND_MOUNT;
	Status routed = route(session, START_OFFSET, message.payload.args.arg1, full, index, rest);
	if (routed != Status::OK) return reply(pid, id, routed);

	if (!(mounts[index].client.*operation)(rest)) return reply(pid, id, failure(index));
	reply(pid, id, Status::OK);
}

void unlinkFile(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	char full[FULL_PATH_BYTES] = {};
	const char *rest = nullptr;
	size_t index = NOT_FOUND_MOUNT;
	Status routed = route(session, START_OFFSET, message.payload.args.arg1, full, index, rest);
	if (routed != Status::OK) return reply(pid, id, routed);

	if (!mounts[index].client.unlink(rest)) return reply(pid, id, failure(index));
	reply(pid, id, Status::OK);
}

void renameEntry(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	char fullFrom[FULL_PATH_BYTES] = {};
	char fullTo[FULL_PATH_BYTES] = {};
	const char *restFrom = nullptr;
	const char *restTo = nullptr;
	size_t fromIndex = NOT_FOUND_MOUNT;
	size_t toIndex = NOT_FOUND_MOUNT;
	Status routed =
		route(session, START_OFFSET, message.payload.args.arg1, fullFrom, fromIndex, restFrom);
	if (routed != Status::OK) return reply(pid, id, routed);
	routed = route(session, cock::abi::fs::RENAME_NEW_OFFSET, message.payload.args.arg2, fullTo,
				   toIndex, restTo);
	if (routed != Status::OK) return reply(pid, id, routed);
	if (fromIndex != toIndex) return reply(pid, id, Status::CROSS_DEVICE);

	if (!mounts[fromIndex].client.rename(restFrom, restTo))
		return reply(pid, id, failure(fromIndex));
	reply(pid, id, Status::OK);
}

void linkEntry(Session &session, const Message &message) {
	uint32_t pid = message.sender_pid;
	uint32_t id = message.payload.args.arg0;

	char fullFrom[FULL_PATH_BYTES] = {};
	char fullTo[FULL_PATH_BYTES] = {};
	const char *restFrom = nullptr;
	const char *restTo = nullptr;
	size_t fromIndex = NOT_FOUND_MOUNT;
	size_t toIndex = NOT_FOUND_MOUNT;
	Status routed =
		route(session, START_OFFSET, message.payload.args.arg1, fullFrom, fromIndex, restFrom);
	if (routed != Status::OK) return reply(pid, id, routed);
	routed = route(session, cock::abi::fs::RENAME_NEW_OFFSET, message.payload.args.arg2, fullTo,
				   toIndex, restTo);
	if (routed != Status::OK) return reply(pid, id, routed);
	if (fromIndex != toIndex) return reply(pid, id, Status::CROSS_DEVICE);

	if (!mounts[fromIndex].client.link(restFrom, restTo))
		return reply(pid, id, failure(fromIndex));
	reply(pid, id, Status::OK);
}

} // namespace cock::vfs
