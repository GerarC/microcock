#include <ata/reply.hpp>
#include <ata/requests.hpp>
#include <ata/sessions.hpp>
#include <cock/abi/block.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/user/session.hpp>

using cock::abi::block::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace cock::ata {

void handle(const Message &message) {
	if (message.reply == cock::abi::endpoint::NO_REPLY) return;
	uint32_t token = message.reply;
	uint32_t client = message.badge;
	uint32_t id = message.payload.args.arg0;

	switch (message.type) {
	case MessageType::BLK_CONNECT:
		return connect(token, client, id);
	case MessageType::BLK_INFO:
	case MessageType::BLK_READ:
	case MessageType::BLK_WRITE:
		break;
	default:
		return;
	}

	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, client);
	if (session == nullptr) return reply(token, id, Status::NOT_CONNECTED);

	if (message.type == MessageType::BLK_INFO) return answerInfo(message);
	if (message.type == MessageType::BLK_WRITE) return answerWrite(*session, message);
	answerRead(*session, message);
}

} // namespace cock::ata
