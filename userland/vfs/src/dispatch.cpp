#include <cock/abi/endpoint.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/session.hpp>
#include <vfs/clients.hpp>
#include <vfs/reply.hpp>
#include <vfs/requests.hpp>

using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::vfs {

void handle(const Message &message) {
	if (message.type == MessageType::PM_FORKED) {
		if (message.badge == cock::abi::service::ROOT_PID)
			inheritSession(message.payload.args.arg1, message.payload.args.arg0);
		return;
	}
	if (message.reply == cock::abi::endpoint::NO_REPLY) return;
	uint32_t client = message.badge;
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;

	switch (message.type) {
	case MessageType::FS_CONNECT:
		return connect(token, client, id, message.payload.args.arg1 == cock::abi::fs::CONNECT_KEEP_FILES);
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
		return writeFile(*session, message);
	case MessageType::FS_UNLINK:
		return unlinkFile(*session, message);
	case MessageType::FS_MKDIR:
		return pathRequest(*session, message, &cock::user::fs::Client::mkdir);
	case MessageType::FS_RMDIR:
		return pathRequest(*session, message, &cock::user::fs::Client::rmdir);
	case MessageType::FS_RENAME:
		return renameEntry(*session, message);
	case MessageType::FS_LINK:
		return linkEntry(*session, message);
	case MessageType::FS_STAT:
		return statFile(*session, message);
	case MessageType::FS_READDIR:
		return listDirectory(*session, message);
	default:
		return closeFile(*session, message);
	}
}

} // namespace cock::vfs
