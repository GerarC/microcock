#include <cock/abi/fs.hpp>
#include <cock/abi/pm.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/spawn.hpp>
#include <rs/directory.hpp>
#include <rs/files.hpp>
#include <rs/pm.hpp>
#include <rs/service.hpp>
#include <rs/spawn.hpp>
#include <stdio.h>
#include <sys/mman.h>

using cock::abi::manifest::Entry;
using cock::user::NO_PROCESS;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr size_t MAX_PROGRAM_BYTES = cock::abi::pm::MAX_IMAGE_BYTES;
constexpr size_t PAGE_BYTES = 0x1000;
constexpr int NO_FILE = -1;

bool fitsProgram(const FileInfo &info) {
	return info.size > cock::rs::NO_BYTES && info.size <= MAX_PROGRAM_BYTES;
}

uint32_t createDirect(Client &client, uint32_t handle, const FileInfo &info, const Entry &entry) {
	cock::abi::service::SpawnRights rights = cock::rs::rightsOf(entry);
	size_t bytes = (info.size + PAGE_BYTES - 1) / PAGE_BYTES * PAGE_BYTES;
	void *image = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, NO_FILE,
					   cock::rs::NO_BYTES);
	if (image == MAP_FAILED) return NO_PROCESS;

	uint32_t pid = NO_PROCESS;
	if (cock::rs::readInto(client, handle, static_cast<uint8_t *>(image), info.size))
		pid = cock::user::spawn(image, info.size, entry.name, false, &rights);
	munmap(image, bytes);
	return pid;
}

uint32_t createThroughPm(Client &client, uint32_t handle, const FileInfo &info, const Entry &entry,
						 bool notify) {
	cock::abi::service::SpawnRights rights = cock::rs::rightsOf(entry);
	uint8_t *image = cock::rs::pmClient().begin(info.size);
	if (image == nullptr) return NO_PROCESS;
	if (!cock::rs::readInto(client, handle, image, info.size)) {
		cock::rs::pmClient().release();
		return NO_PROCESS;
	}
	return cock::rs::pmClient().finish(entry.name, rights, cock::rs::flagsOf(entry, notify), info.size);
}

} // namespace

namespace cock::rs {

uint32_t spawnProgram(Client &client, const Entry &entry, bool notify) {
	uint32_t handle = client.open(entry.path);
	if (handle == cock::abi::fs::NO_HANDLE) {
		printf("rs: cannot open %s\n", entry.path);
		return NO_PROCESS;
	}

	uint32_t pid = NO_PROCESS;
	FileInfo info = {};
	if (client.stat(handle, info) && fitsProgram(info))
		pid = createThroughPm(client, handle, info, entry, notify);
	client.close(handle);

	if (pid == NO_PROCESS) {
		printf("rs: cannot start %s\n", entry.name);
		return pid;
	}
	bindService(labelOf(entry), pid);
	printf("rs: started %s\n", entry.name);
	return pid;
}

uint32_t spawnDirect(Client &client, const Entry &entry) {
	uint32_t handle = client.open(entry.path);
	if (handle == cock::abi::fs::NO_HANDLE) {
		printf("rs: cannot open %s\n", entry.path);
		return NO_PROCESS;
	}

	uint32_t pid = NO_PROCESS;
	FileInfo info = {};
	if (client.stat(handle, info) && fitsProgram(info)) pid = createDirect(client, handle, info, entry);
	client.close(handle);

	if (pid == NO_PROCESS) {
		printf("rs: cannot start %s\n", entry.name);
		return pid;
	}
	printf("rs: started %s\n", entry.name);
	return pid;
}

} // namespace cock::rs
