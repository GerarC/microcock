#include <stdio.h>
#include <string.h>
#include <vfs/limits.hpp>
#include <vfs/mounts.hpp>
#include <vfs/opens.hpp>
#include <vfs/pending.hpp>

using cock::abi::fs::Status;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace cock::vfs {

namespace {

constexpr long MORE_TO_READ = 1;
constexpr size_t NO_MOUNTS = 0;
constexpr size_t NO_LENGTH = 0;

char mountsText[cock::abi::mounts::MAX_FILE_BYTES];
cock::abi::mounts::Table table;

bool readInto(Client &client, uint32_t handle, char *destination, uint32_t size) {
	uint32_t copied = START_OFFSET;
	long step = MORE_TO_READ;
	while (copied < size && step > 0) {
		step = client.read(handle, copied, destination + copied, size - copied);
		if (step > 0) copied += static_cast<uint32_t>(step);
	}
	return copied == size;
}

} // namespace

Mount mounts[cock::abi::mounts::MAX_MOUNTS];
size_t mountCount = NO_MOUNTS;

bool loadMounts() {
	Client client;
	client.onForeign = defer;
	if (!client.connect(cock::abi::fs::RAMFS_SERVER_NAME)) return false;

	uint32_t handle = client.open(cock::abi::mounts::FILE_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool loaded = client.stat(handle, info) && info.size > START_OFFSET &&
				  info.size <= sizeof(mountsText) &&
				  readInto(client, handle, mountsText, info.size);
	client.close(handle);
	if (!loaded) return false;

	if (!table.parse(mountsText, info.size)) {
		printf("vfs: %s line %u rejected, error %u\n", cock::abi::mounts::FILE_PATH,
			   static_cast<unsigned>(table.errorLine), static_cast<unsigned>(table.error));
		return false;
	}

	for (size_t index = 0; index < table.count; index++) {
		Mount &mount = mounts[index];
		memcpy(mount.prefix, table.mounts[index].prefix, sizeof(mount.prefix));
		memcpy(mount.server, table.mounts[index].server, sizeof(mount.server));
		mount.prefixLength = strlen(mount.prefix);
	}
	mountCount = table.count;
	return true;
}

size_t pickMount(const char *full, const char *&rest) {
	size_t best = NOT_FOUND_MOUNT;
	size_t bestLength = NO_LENGTH;
	size_t fullLength = strlen(full);
	for (size_t index = 0; index < mountCount; index++) {
		size_t length = mounts[index].prefixLength;
		if (length < bestLength || fullLength < length) continue;
		if (memcmp(full, mounts[index].prefix, length) != 0) continue;

		bool whole = length == SEPARATOR_BYTES || full[length] == '\0' ||
					 full[length] == PATH_SEPARATOR;
		if (!whole) continue;
		best = index;
		bestLength = length;
	}

	if (best == NOT_FOUND_MOUNT) return best;
	rest = bestLength == SEPARATOR_BYTES ? full : full + bestLength;
	if (*rest == '\0') rest = cock::abi::mounts::ROOT_PREFIX;
	return best;
}

bool ensureMount(size_t index) {
	Mount &mount = mounts[index];
	if (!mount.connected) {
		mount.client.onForeign = defer;
		mount.connected = mount.client.connect(mount.server);
	}
	return mount.connected;
}

void markDown(size_t index) {
	mounts[index].connected = false;
	for (Open &open : opens)
		if (open.used && open.mount == index) memset(&open, 0, sizeof(open));
}

Status failure(size_t index) {
	Status status = mounts[index].client.lastStatus();
	if (status == Status::NOT_CONNECTED) markDown(index);
	return status == Status::OK ? Status::IO_ERROR : status;
}

bool isMountPoint(const Mount &mount, const char *&name) {
	if (mount.prefixLength <= SEPARATOR_BYTES) return false;
	name = mount.prefix + SEPARATOR_BYTES;
	for (const char *cursor = name; *cursor != '\0'; cursor++)
		if (*cursor == PATH_SEPARATOR) return false;
	return true;
}

} // namespace cock::vfs
