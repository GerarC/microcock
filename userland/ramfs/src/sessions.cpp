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

void connect(uint32_t pid, uint32_t id) {
	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, pid);
	if (session != nullptr) {
		memset(session->open, 0, sizeof(session->open));
		reply(pid, id, Status::OK, session->windowId);
		return;
	}

	releaseDeadSessions();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr ||
		!cock::user::openSession(*session, pid, cock::abi::fs::WINDOW_PAGES))
		return reply(pid, id, Status::NO_SPACE);
	reply(pid, id, Status::OK, session->windowId);
}

const Entry *handleEntry(const Session &session, uint32_t handle) {
	if (handle == cock::abi::fs::NO_HANDLE || handle > MAX_HANDLES) return nullptr;
	size_t slot = handle - 1;
	return session.open[slot] ? &entries[session.entry[slot]] : nullptr;
}

} // namespace cock::ramfs
