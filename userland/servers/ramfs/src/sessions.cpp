#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/session.hpp>
#include <ramfs/reply.hpp>
#include <ramfs/sessions.hpp>
#include <string.h>

using cock::abi::fs::Status;

namespace cock::ramfs {

Session sessions[MAX_CLIENTS];

void releaseDeadSessions() {
	for (Session &session : sessions)
		if (session.used && !cock::user::process_alive(session.pid))
			cock::user::releaseSession(session, cock::abi::fs::WINDOW_BYTES);
}

void connect(uint32_t token, uint32_t client, uint32_t id) {
	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, client);
	if (session != nullptr) {
		memset(session->open, 0, sizeof(session->open));
		reply(token, id, Status::OK, session->windowId);
		return;
	}

	releaseDeadSessions();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr ||
		!cock::user::openSession(*session, client, cock::abi::fs::WINDOW_PAGES))
		return reply(token, id, Status::NO_SPACE);
	reply(token, id, Status::OK, session->windowId);
}

const Entry *handleEntry(const Session &session, uint32_t handle) {
	if (handle == cock::abi::fs::NO_HANDLE || handle > MAX_HANDLES) return nullptr;
	size_t slot = handle - 1;
	return session.open[slot] ? &entries[session.entry[slot]] : nullptr;
}

} // namespace cock::ramfs
