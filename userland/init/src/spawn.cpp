#include <cock/abi/fs.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/spawn.hpp>
#include <init/files.hpp>
#include <init/service.hpp>
#include <init/spawn.hpp>
#include <stdio.h>
#include <sys/mman.h>

using cock::abi::manifest::Entry;
using cock::user::NO_PROCESS;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr size_t MAX_PROGRAM_BYTES = 0x100000;
constexpr size_t PAGE_BYTES = 0x1000;
constexpr int NO_FILE = -1;

} // namespace

namespace cock::init {

uint32_t spawnProgram(Client &client, const Entry &entry, bool notify) {
	uint32_t handle = client.open(entry.path);
	if (handle == cock::abi::fs::NO_HANDLE) {
		printf("init: cannot open %s\n", entry.path);
		return NO_PROCESS;
	}

	uint32_t pid = NO_PROCESS;
	cock::abi::service::SpawnRights rights = rightsOf(entry);
	FileInfo info = {};
	if (client.stat(handle, info) && info.size > NO_BYTES && info.size <= MAX_PROGRAM_BYTES) {
		size_t bytes = (info.size + PAGE_BYTES - 1) / PAGE_BYTES * PAGE_BYTES;
		void *image = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
						   NO_FILE, NO_BYTES);
		if (image != MAP_FAILED) {
			if (readInto(client, handle, static_cast<uint8_t *>(image), info.size))
				pid = cock::user::spawn(image, info.size, entry.name, notify, &rights);
			munmap(image, bytes);
		}
	}
	client.close(handle);

	if (pid == NO_PROCESS)
		printf("init: cannot start %s\n", entry.name);
	else
		printf("init: started %s\n", entry.name);
	return pid;
}

} // namespace cock::init
