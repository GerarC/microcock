#include <ata/devices.hpp>
#include <ata/reply.hpp>
#include <ata/sessions.hpp>
#include <cock/abi/block.hpp>
#include <cock/user/api.hpp>
#include <cock/user/session.hpp>

using cock::abi::block::Status;

namespace cock::ata {

Session sessions[MAX_CLIENTS];

void releaseDeadSessions() {
	for (Session &session : sessions)
		if (session.used && !cock::user::process_alive(session.pid))
			cock::user::releaseSession(session, cock::abi::block::WINDOW_BYTES);
}

void connect(uint32_t pid, uint32_t id) {
	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, pid);
	if (session != nullptr) return reply(pid, id, Status::OK, session->windowId, deviceCount);

	releaseDeadSessions();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr ||
		!cock::user::openSession(*session, pid, cock::abi::block::WINDOW_PAGES))
		return reply(pid, id, Status::NO_SPACE);
	reply(pid, id, Status::OK, session->windowId, deviceCount);
}

} // namespace cock::ata
