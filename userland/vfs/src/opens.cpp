#include <cock/abi/fs.hpp>
#include <string.h>
#include <vfs/mounts.hpp>
#include <vfs/opens.hpp>

namespace cock::vfs {

Open opens[MAX_OPEN];

Open *findOpen(uint32_t pid, uint32_t number) {
	if (number == cock::abi::fs::NO_HANDLE || number > MAX_OPEN) return nullptr;
	Open &open = opens[number - 1];
	return open.used && open.owner == pid ? &open : nullptr;
}

Open *freeOpen() {
	for (Open &open : opens)
		if (!open.used) return &open;
	return nullptr;
}

void closeOpen(Open &open) {
	mounts[open.mount].client.close(open.backend);
	memset(&open, 0, sizeof(open));
}

void closeOwnedBy(uint32_t pid) {
	for (Open &open : opens)
		if (open.used && open.owner == pid) closeOpen(open);
}

} // namespace cock::vfs
