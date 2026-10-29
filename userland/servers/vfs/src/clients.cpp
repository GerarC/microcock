#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/session.hpp>
#include <vfs/clients.hpp>
#include <vfs/mounts.hpp>
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

bool releasedAfterBusy(size_t index) {
	if (mounts[index].client.lastStatus() != Status::BUSY) return false;
	releaseDead();
	return true;
}

void connect(uint32_t token, uint32_t client, uint32_t id, bool keepFiles) {
	Session *session = cock::user::findSession(sessions, MAX_CLIENTS, client);
	if (session != nullptr) {
		if (!cock::user::shm_share(session->windowId, client, true)) return reply(token, id, Status::NO_SPACE);
		if (session->inherited)
			session->inherited = false;
		else if (!keepFiles)
			closeOwnedBy(client);
		return reply(token, id, Status::OK, session->windowId);
	}

	releaseDead();
	session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr ||
		!cock::user::openSession(*session, client, cock::abi::fs::WINDOW_PAGES))
		return reply(token, id, Status::NO_SPACE);
	reply(token, id, Status::OK, session->windowId);
}

void inheritSession(uint32_t parent, uint32_t child) {
	if (cock::user::findSession(sessions, MAX_CLIENTS, parent) == nullptr ||
		cock::user::findSession(sessions, MAX_CLIENTS, child) != nullptr)
		return;

	Session *session = cock::user::freeSession(sessions, MAX_CLIENTS);
	if (session == nullptr) {
		releaseDead();
		session = cock::user::freeSession(sessions, MAX_CLIENTS);
	}
	if (session == nullptr || !cock::user::openSession(*session, child, cock::abi::fs::WINDOW_PAGES)) return;

	session->inherited = true;
	copyOpens(parent, child);
}

} // namespace cock::vfs
