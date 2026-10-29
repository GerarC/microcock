#include <cock/abi/endpoint.hpp>
#include <cock/user/session.hpp>
#include <ramfs/reply.hpp>
#include <ramfs/requests.hpp>
#include <ramfs/sessions.hpp>

using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::ramfs {

void handle(const Message &message) {
	if (message.reply == cock::abi::endpoint::NO_REPLY) return;
	uint32_t client = message.badge;
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;

	switch (message.type) {
	case MessageType::FS_CONNECT:
		return connect(token, client, id);
	case MessageType::FS_OPEN:
	case MessageType::FS_READ:
	case MessageType::FS_WRITE:
	case MessageType::FS_UNLINK:
	case MessageType::FS_MKDIR:
	case MessageType::FS_RMDIR:
	case MessageType::FS_RENAME:
	case MessageType::FS_LINK:
	case MessageType::FS_STAT:
	case MessageType::FS_READDIR:
	case MessageType::FS_CLOSE:
		break;
	default:
		return;
	}

	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, client);
	if (session == nullptr) return reply(token, id, Status::NOT_CONNECTED);

	switch (message.type) {
	case MessageType::FS_OPEN:
		return openFile(*session, message);
	case MessageType::FS_READ:
		return readFile(*session, message);
	case MessageType::FS_WRITE:
	case MessageType::FS_UNLINK:
	case MessageType::FS_MKDIR:
	case MessageType::FS_RMDIR:
	case MessageType::FS_RENAME:
	case MessageType::FS_LINK:
		return reply(token, id, Status::READ_ONLY);
	case MessageType::FS_STAT:
		return statFile(*session, message);
	case MessageType::FS_READDIR:
		return listDirectory(*session, message);
	default:
		return closeFile(*session, message);
	}
}

} // namespace cock::ramfs
