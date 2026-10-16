/**
 * @file session.hpp
 * @brief Helpers for servers that keep one session per client in a fixed table.
 *
 * Each session type must have the fields `used` (bool), `pid` (uint32_t), `windowId`
 * (uint32_t) and `window` (pointer to bytes), and must be safe to clear with `memset`.
 * A session owns a shared memory window that the client can map.
 */
#ifndef COCK_USER_SESSION_HPP
#define COCK_USER_SESSION_HPP

#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

namespace cock::user {

/**
 * @brief Finds the session of a client.
 * @tparam Session Session type (see the file description).
 * @param table First entry of the table.
 * @param count Number of entries.
 * @param pid Client process id.
 * @return The used entry with that pid, or nullptr when there is none.
 */
template <typename Session>
Session *findSession(Session *table, size_t count, uint32_t pid) {
	for (size_t index = 0; index < count; index++)
		if (table[index].used && table[index].pid == pid) return &table[index];
	return nullptr;
}

/**
 * @brief Finds an unused entry.
 * @tparam Session Session type.
 * @param table First entry of the table.
 * @param count Number of entries.
 * @return The first entry that is not used, or nullptr when the table is full.
 */
template <typename Session>
Session *freeSession(Session *table, size_t count) {
	for (size_t index = 0; index < count; index++)
		if (!table[index].used) return &table[index];
	return nullptr;
}

/**
 * @brief Opens a session: creates a shared window, shares it writable with the client and maps it.
 *
 * Uses `SYS_SHM_CREATE`, `SYS_SHM_SHARE` and `SYS_SHM_MAP`. On failure the object is
 * released and the session is left untouched.
 * @tparam Session Session type.
 * @param session Entry to fill; cleared first, then marked used.
 * @param pid Client process id that gets write access to the window.
 * @param pages Window size in pages.
 * @return true when the session is ready; false when the object could not be created,
 *         shared (client not alive) or mapped.
 */
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

/**
 * @brief Closes a session: unmaps and releases the window and clears the entry.
 * @tparam Session Session type.
 * @param session Open session.
 * @param windowBytes Size of the window in bytes, as given to `openSession` (pages times page size).
 */
template <typename Session>
void releaseSession(Session &session, size_t windowBytes) {
	munmap(session.window, windowBytes);
	shm_release(session.windowId);
	memset(&session, 0, sizeof(session));
}

} // namespace cock::user

#endif // !COCK_USER_SESSION_HPP
