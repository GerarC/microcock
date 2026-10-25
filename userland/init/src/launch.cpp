#include <cock/abi/fs.hpp>
#include <cock/abi/pm.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <init/launch.hpp>
#include <stdio.h>
#include <string.h>

using cock::abi::manifest::Entry;
using cock::user::NO_PROCESS;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr char ROOT_PATH[] = "/";
constexpr size_t NO_BYTES = 0;
constexpr uint32_t NO_FLAGS = 0;

struct Path {
	char text[sizeof(ROOT_PATH) + cock::abi::manifest::MAX_PATH_BYTES];
};

Path absolute(const Entry &entry) {
	Path path = {};
	size_t root = sizeof(ROOT_PATH) - 1;
	memcpy(path.text, ROOT_PATH, root);
	memcpy(path.text + root, entry.path, strlen(entry.path));
	return path;
}

} // namespace

namespace cock::init {

uint32_t launch(Client &files, cock::user::pm::Client &process, const Entry &entry, size_t index) {
	Path path = absolute(entry);
	uint32_t handle = files.open(path.text);
	if (handle == cock::abi::fs::NO_HANDLE) {
		printf("init: cannot open %s\n", path.text);
		return NO_PROCESS;
	}

	uint32_t pid = NO_PROCESS;
	FileInfo info = {};
	if (files.stat(handle, info) && info.size > NO_BYTES && info.size <= cock::abi::pm::MAX_IMAGE_BYTES) {
		uint8_t *image = process.begin(info.size);
		if (image != nullptr && files.readAll(handle, image, info.size)) {
			cock::abi::service::SpawnRights rights = {};
			rights.label = cock::abi::service::programLabel(index);
			pid = process.finish(entry.name, rights, NO_FLAGS, info.size);
		} else {
			process.release();
		}
	}
	files.close(handle);

	if (pid == NO_PROCESS) {
		printf("init: cannot start %s\n", entry.name);
		return pid;
	}
	printf("init: started %s\n", entry.name);
	return pid;
}

} // namespace cock::init
