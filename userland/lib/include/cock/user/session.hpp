#ifndef COCK_USER_SESSION_HPP
#define COCK_USER_SESSION_HPP

#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

namespace cock::user {

template <typename Session>
Session *findSession(Session *table, size_t count, uint32_t pid) {
	for (size_t index = 0; index < count; index++)
		if (table[index].used && table[index].pid == pid) return &table[index];
	return nullptr;
}

template <typename Session>
Session *freeSession(Session *table, size_t count) {
	for (size_t index = 0; index < count; index++)
		if (!table[index].used) return &table[index];
	return nullptr;
}

template <typename Session>
bool openSession(Session &session, uint32_t pid, size_t pages) {
	uint32_t window = shm_create(pages);
	if (window == NO_SHM) return false;

	uint8_t *view = nullptr;
	if (shm_share(window, pid, true)) view = static_cast<uint8_t *>(shm_map(window, true));
	if (view == nullptr) {
		shm_release(window);
		return false;
	}

	memset(&session, 0, sizeof(session));
	session.used = true;
	session.pid = pid;
	session.windowId = window;
	session.window = view;
	return true;
}

template <typename Session>
void releaseSession(Session &session, size_t windowBytes) {
	munmap(session.window, windowBytes);
	shm_release(session.windowId);
	memset(&session, 0, sizeof(session));
}

} // namespace cock::user

#endif // !COCK_USER_SESSION_HPP
