#include <cock/abi/capability.hpp>
#include <cock/abi/fs.hpp>
#include <cock/abi/manifest.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <stdio.h>
#include <sys/mman.h>

using cock::abi::manifest::Entry;
using cock::abi::manifest::Manifest;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr size_t MAX_PROGRAM_BYTES = 0x100000;
constexpr size_t PAGE_BYTES = 0x1000;
constexpr uintptr_t SINGLE_RESOURCE = 1;
constexpr uintptr_t EXIT_SUCCESS_STATUS = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr int NO_FILE = -1;
constexpr size_t NO_BYTES = 0;
constexpr uint32_t START_OFFSET = 0;
constexpr long MORE_TO_READ = 1;

char manifestText[cock::abi::manifest::MAX_FILE_BYTES];
Manifest manifest;

void announce(uint32_t pid, uint32_t peer) {
	Message message = {};
	message.type = MessageType::CAPABILITY_GRANTED;
	message.payload.args.arg0 = peer;
	cock::user::ipc_send(pid, message);
}

void yieldCpu() { syscall(SYS_SCHED_YIELD, 0, 0, 0, 0, 0, 0); }

bool readInto(Client &client, uint32_t handle, uint8_t *destination, uint32_t size) {
	uint32_t copied = START_OFFSET;
	long step = MORE_TO_READ;
	while (copied < size && step > 0) {
		step = client.read(handle, copied, destination + copied, size - copied);
		if (step > 0) copied += static_cast<uint32_t>(step);
	}
	return copied == size;
}

bool loadManifest(Client &client, size_t &length) {
	uint32_t handle = client.open(cock::abi::manifest::FILE_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool loaded = client.stat(handle, info) && info.size > NO_BYTES &&
				  info.size <= sizeof(manifestText) &&
				  readInto(client, handle, reinterpret_cast<uint8_t *>(manifestText),
						   info.size);
	client.close(handle);
	length = info.size;
	return loaded;
}

uint32_t spawnProgram(Client &client, const Entry &entry) {
	uint32_t handle = client.open(entry.path);
	if (handle == cock::abi::fs::NO_HANDLE) {
		printf("init: cannot open %s\n", entry.path);
		return NO_PROCESS;
	}

	uint32_t pid = NO_PROCESS;
	FileInfo info = {};
	if (client.stat(handle, info) && info.size > NO_BYTES &&
		info.size <= MAX_PROGRAM_BYTES) {
		size_t bytes = (info.size + PAGE_BYTES - 1) / PAGE_BYTES * PAGE_BYTES;
		void *image = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
						   MAP_PRIVATE | MAP_ANONYMOUS, NO_FILE, NO_BYTES);
		if (image != MAP_FAILED) {
			if (readInto(client, handle, static_cast<uint8_t *>(image), info.size))
				pid = cock::user::spawn(image, info.size, entry.name);
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

void equip(const Entry &entry, uint32_t pid) {
	for (size_t index = 0; index < entry.grantCount; index++) {
		const cock::abi::manifest::Grant &grant = entry.grants[index];
		cock::user::cap_grant(pid, grant.type, grant.first, grant.count);
	}

	if (entry.grantCount == 0 && !entry.hasPeer()) return;
	uint32_t peer = entry.hasPeer() ? cock::user::lookup(entry.peer) : NO_PROCESS;
	announce(pid, peer);
}

void runEntry(Client &client, const Entry &entry) {
	if (entry.isBootModule()) {
		uint32_t pid = cock::user::lookup(entry.name);
		if (pid == NO_PROCESS) {
			printf("init: no boot module %s\n", entry.name);
			return;
		}
		equip(entry, pid);
		printf("init: configured %s\n", entry.name);
		return;
	}

	for (uint32_t run = 0; run < entry.repeat; run++) {
		uint32_t pid = spawnProgram(client, entry);
		if (pid == NO_PROCESS) return;
		equip(entry, pid);
		while (entry.repeat > cock::abi::manifest::DEFAULT_REPEAT &&
			   cock::user::process_alive(pid))
			yieldCpu();
	}
}

} // namespace

extern "C" void _start() {
	uint32_t files = cock::user::lookup(cock::abi::fs::RAMFS_SERVER_NAME);
	uint32_t initrd = cock::user::module_index(cock::abi::fs::INITRD_MODULE_NAME);
	if (files == NO_PROCESS || initrd == cock::user::NO_MODULE)
		cock::user::exit_process(EXIT_FAILED_STATUS);

	printf("init: initrd is boot module %u\n", initrd);
	cock::user::cap_grant(files, cock::abi::CapabilityType::BOOT_MODULE, initrd,
						  SINGLE_RESOURCE);
	announce(files, NO_PROCESS);

	Client client;
	size_t length = NO_BYTES;
	if (!client.connect(cock::abi::fs::RAMFS_SERVER_NAME) || !loadManifest(client, length)) {
		printf("init: cannot read %s\n", cock::abi::manifest::FILE_PATH);
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}

	if (!manifest.parse(manifestText, length)) {
		printf("init: manifest line %u rejected, error %u\n",
			   static_cast<unsigned>(manifest.errorLine),
			   static_cast<unsigned>(manifest.error));
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("init: manifest has %u entries\n", static_cast<unsigned>(manifest.count));

	for (size_t index = 0; index < manifest.count; index++)
		runEntry(client, manifest.entries[index]);

	cock::user::exit_process(EXIT_SUCCESS_STATUS);
}
