#include <ata/pending.hpp>
#include <ata/requests.hpp>
#include <stddef.h>

namespace cock::ata {

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
	for (size_t index = 0; index < pendingCount; index++)
		handle(pending[index]);
	pendingCount = NO_PENDING;
}

} // namespace cock::ata
