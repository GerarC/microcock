#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/session.hpp>
#include <vfs/clients.hpp>
#include <vfs/opens.hpp>
#include <vfs/reply.hpp>

using cock::abi::fs::Status;

namespace cock::vfs {

Session sessions[MAX_CLIENTS];

void releaseDead() {
	for (Open &open : opens)
		if (open.used && !cock::user::process_alive(open.owner)) closeOpen(open);

	for (Session &session : sessions)
		if (session.used && !cock::user::process_alive(session.pid))
			cock::user::releaseSession(session, cock::abi::fs::WINDOW_BYTES);
}

void connect(uint32_t pid, uint32_t id) {
	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, pid);
	if (session != nullptr) {
		closeOwnedBy(pid);
		return reply(pid, id, Status::OK, session->windowId);
	}

	releaseDead();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr ||
		!cock::user::openSession(*session, pid, cock::abi::fs::WINDOW_PAGES))
		return reply(pid, id, Status::NO_SPACE);
	reply(pid, id, Status::OK, session->windowId);
}

} // namespace cock::vfs
