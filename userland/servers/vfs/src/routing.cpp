#include <string.h>
#include <vfs/mounts.hpp>
#include <vfs/routing.hpp>

using cock::abi::fs::Status;

namespace cock::vfs {

Status route(const Session &session, size_t at, uint32_t length, char *full, size_t &index,
			 const char *&rest) {
	if (length == 0 || length >= cock::abi::fs::MAX_PATH_BYTES) return Status::BAD_REQUEST;

	char path[cock::abi::fs::MAX_PATH_BYTES] = {};
	memcpy(path, session.window + at, length);
	if (strlen(path) != length) return Status::BAD_REQUEST;

	size_t offset = START_OFFSET;
	if (path[0] != PATH_SEPARATOR) full[offset++] = PATH_SEPARATOR;
	memcpy(full + offset, path, length);

	index = pickMount(full, rest);
	if (index == NOT_FOUND_MOUNT || strlen(rest) >= cock::abi::fs::MAX_PATH_BYTES)
		return Status::BAD_REQUEST;
	return ensureMount(index) ? Status::OK : Status::NOT_FOUND;
}

} // namespace cock::vfs
