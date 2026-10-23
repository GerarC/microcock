#include <ramfs/pending.hpp>
#include <ramfs/requests.hpp>
#include <stddef.h>

namespace cock::ramfs {

namespace {

constexpr size_t MAX_PENDING = 16;
constexpr size_t NO_PENDING = 0;

cock::ipc::Message pending[MAX_PENDING];
size_t pendingCount = NO_PENDING;

} // namespace

void keepPending(const cock::ipc::Message &message) {
	if (pendingCount < MAX_PENDING) pending[pendingCount++] = message;
}

void handlePending() {
	for (size_t idx = 0; idx < pendingCount; idx++)
		handle(pending[idx]);
	pendingCount = NO_PENDING;
}

} // namespace cock::ramfs
