#include <stddef.h>
#include <vfs/pending.hpp>

namespace cock::vfs {

namespace {

constexpr size_t MAX_PENDING = 16;
constexpr size_t FIRST_PENDING = 0;
constexpr size_t NO_PENDING = 0;

cock::ipc::Message pending[MAX_PENDING];
size_t pendingCount = NO_PENDING;

} // namespace

void defer(const cock::ipc::Message &message) {
	if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
}

bool takePending(cock::ipc::Message &message) {
	if (pendingCount == NO_PENDING) return false;

	message = pending[FIRST_PENDING];
	pendingCount--;
	for (size_t index = 0; index < pendingCount; index++)
		pending[index] = pending[index + 1];
	return true;
}

} // namespace cock::vfs
