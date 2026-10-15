#include "probe.hpp"
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

namespace probe {

namespace {

constexpr size_t SHARED_PAGES = 2;
constexpr size_t SHARED_OVERSIZED_PAGES = 4096;
constexpr size_t SHARED_ATTEMPTS = 128;
constexpr uint32_t UNKNOWN_SHM_ID = 0xFFFF;
constexpr uint32_t UNKNOWN_PID = 0x7FFFFFF0;

} // namespace

bool sharedMemoryRoundTrips() {
	uint32_t object = cock::user::shm_create(SHARED_PAGES);
	if (object == cock::user::NO_SHM) return false;

	uint8_t *view = static_cast<uint8_t *>(cock::user::shm_map(object, true));
	bool ok = view != nullptr;
	for (size_t idx = 0; ok && idx < SHARED_PAGES * PAGE_BYTES; idx++)
		ok = view[idx] == ZERO_PATTERN;

	if (ok) view[SHARED_PAGES * PAGE_BYTES - 1] = FIRST_PATTERN;
	ok = ok && munmap(view, SHARED_PAGES * PAGE_BYTES) == 0;

	uint8_t *again = static_cast<uint8_t *>(cock::user::shm_map(object, false));
	ok = ok && again != nullptr &&
		 again[SHARED_PAGES * PAGE_BYTES - 1] == FIRST_PATTERN;
	ok = ok && munmap(again, SHARED_PAGES * PAGE_BYTES) == 0;

	ok = ok && cock::user::shm_share(object, cock::user::self(), true);
	ok = ok && cock::user::shm_release(object);
	return ok && cock::user::shm_map(object, false) == nullptr;
}

bool sharedMemoryRefusesBadRequests() {
	uint32_t object = cock::user::shm_create(SHARED_PAGES);
	if (object == cock::user::NO_SHM) return false;

	bool ok = cock::user::shm_create(NO_PAGES) == cock::user::NO_SHM &&
			  cock::user::shm_create(SHARED_OVERSIZED_PAGES) == cock::user::NO_SHM &&
			  cock::user::shm_map(UNKNOWN_SHM_ID, false) == nullptr &&
			  !cock::user::shm_release(UNKNOWN_SHM_ID) &&
			  !cock::user::shm_share(UNKNOWN_SHM_ID, cock::user::self(), false) &&
			  !cock::user::shm_share(object, UNKNOWN_PID, false);
	return cock::user::shm_release(object) && ok;
}

bool sharedMemoryTableIsBounded() {
	uint32_t objects[SHARED_ATTEMPTS];
	size_t created = 0;

	while (created < SHARED_ATTEMPTS) {
		uint32_t object = cock::user::shm_create(1);
		if (object == cock::user::NO_SHM) break;
		objects[created++] = object;
	}
	bool bounded = created > 0 && created < SHARED_ATTEMPTS;

	for (size_t idx = 0; idx < created; idx++)
		cock::user::shm_release(objects[idx]);

	uint32_t after = cock::user::shm_create(1);
	return bounded && after != cock::user::NO_SHM && cock::user::shm_release(after);
}

} // namespace probe
