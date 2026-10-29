#include <cock/abi/fs.hpp>
#include <string.h>
#include <vfs/mounts.hpp>
#include <vfs/opens.hpp>

namespace cock::vfs {

namespace {

constexpr uint32_t FIRST_HANDLE = 1;
constexpr uint32_t NEXT = 1;

bool namedByAnother(const Open &open) {
	for (const Open &other : opens)
		if (&other != &open && other.used && other.mount == open.mount && other.backend == open.backend)
			return true;
	return false;
}

} // namespace

Open opens[MAX_OPEN];

Open *findOpen(uint32_t pid, uint32_t number) {
	if (number == cock::abi::fs::NO_HANDLE) return nullptr;
	for (Open &open : opens)
		if (open.used && open.owner == pid && open.handle == number) return &open;
	return nullptr;
}

Open *freeOpen() {
	for (Open &open : opens)
		if (!open.used) return &open;
	return nullptr;
}

uint32_t newHandle(uint32_t pid) {
	uint32_t handle = FIRST_HANDLE;
	while (findOpen(pid, handle) != nullptr)
		handle += NEXT;
	return handle;
}

bool releaseOpen(Open &open) {
	bool closed = namedByAnother(open) || mounts[open.mount].client.close(open.backend);
	memset(&open, 0, sizeof(open));
	return closed;
}

void closeOpen(Open &open) {
	releaseOpen(open);
}

void closeOwnedBy(uint32_t pid) {
	for (Open &open : opens)
		if (open.used && open.owner == pid) closeOpen(open);
}

void copyOpens(uint32_t parent, uint32_t child) {
	for (size_t index = 0; index < MAX_OPEN; index++) {
		Open original = opens[index];
		if (!original.used || original.owner != parent) continue;

		Open *twin = freeOpen();
		if (twin == nullptr) return;
		*twin = original;
		twin->owner = child;
	}
}

} // namespace cock::vfs
