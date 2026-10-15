#include <cock/abi/capability.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <stdio.h>
#include <sys/mman.h>

using cock::abi::CapabilityType;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::NO_PROCESS;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

struct Program {
	const char *path;
	const char *name;
};

constexpr const char *VGA_DRIVER = "vga_driver";
constexpr Program EARLY_PROGRAMS[] = {{"bin/aborter.elf", "aborter"},
									  {"bin/hello.elf", "hello"}};
constexpr Program KEYBOARD_PROGRAM = {"bin/kbd.elf", "kbd"};
constexpr Program FILE_CLIENT_PROGRAM = {"bin/fsclient.elf", "fsclient"};
constexpr Program LAST_PROGRAM = {"bin/probe.elf", "probe"};
constexpr size_t FILE_CLIENT_RUNS = 10;
constexpr size_t MAX_PROGRAM_BYTES = 0x100000;
constexpr size_t PAGE_BYTES = 0x1000;
constexpr uintptr_t VIDEO_WINDOW_START = 0xA0000;
constexpr uintptr_t VIDEO_WINDOW_SIZE = 0x20000;
constexpr uintptr_t PS2_DATA_PORT = 0x60;
constexpr uintptr_t KEYBOARD_IRQ_LINE = 1;
constexpr uintptr_t SINGLE_RESOURCE = 1;
constexpr uintptr_t EXIT_SUCCESS_STATUS = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr int NO_FILE = -1;
constexpr size_t NO_BYTES = 0;
constexpr uint32_t START_OFFSET = 0;
constexpr long MORE_TO_READ = 1;

void announce(uint32_t pid, uint32_t peer) {
	Message message = {};
	message.type = MessageType::CAPABILITY_GRANTED;
	message.payload.args.arg0 = peer;
	cock::user::ipc_send(pid, message);
}

void yieldCpu() { syscall(SYS_SCHED_YIELD, 0, 0, 0, 0, 0, 0); }

uint32_t spawnProgram(Client &client, const Program &program) {
	uint32_t handle = client.open(program.path);
	if (handle == cock::abi::fs::NO_HANDLE) {
		printf("init: cannot open %s\n", program.path);
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
			uint8_t *cursor = static_cast<uint8_t *>(image);
			uint32_t copied = START_OFFSET;
			long step = MORE_TO_READ;
			while (copied < info.size && step > 0) {
				step = client.read(handle, copied, cursor + copied, info.size - copied);
				if (step > 0) copied += static_cast<uint32_t>(step);
			}
			if (copied == info.size) pid = cock::user::spawn(image, info.size, program.name);
			munmap(image, bytes);
		}
	}
	client.close(handle);

	if (pid == NO_PROCESS)
		printf("init: cannot start %s\n", program.name);
	else
		printf("init: started %s\n", program.name);
	return pid;
}

void runFileClients(Client &client) {
	for (size_t run = 0; run < FILE_CLIENT_RUNS; run++) {
		uint32_t pid = spawnProgram(client, FILE_CLIENT_PROGRAM);
		while (pid != NO_PROCESS && cock::user::process_alive(pid))
			yieldCpu();
	}
}

} // namespace

extern "C" void _start() {
	uint32_t display = cock::user::lookup(VGA_DRIVER);
	uint32_t files = cock::user::lookup(cock::abi::fs::SERVER_NAME);
	uint32_t initrd = cock::user::module_index(cock::abi::fs::INITRD_MODULE_NAME);

	if (display != NO_PROCESS) {
		cock::user::cap_grant(display, CapabilityType::PHYSICAL_MEMORY,
							  VIDEO_WINDOW_START, VIDEO_WINDOW_SIZE);
		announce(display, display);
	}

	if (files == NO_PROCESS || initrd == cock::user::NO_MODULE)
		cock::user::exit_process(EXIT_FAILED_STATUS);

	printf("init: initrd is boot module %u\n", initrd);
	cock::user::cap_grant(files, CapabilityType::BOOT_MODULE, initrd, SINGLE_RESOURCE);
	announce(files, NO_PROCESS);

	Client client;
	if (!client.connect()) cock::user::exit_process(EXIT_FAILED_STATUS);

	runFileClients(client);

	for (const Program &program : EARLY_PROGRAMS)
		spawnProgram(client, program);

	uint32_t keyboard = spawnProgram(client, KEYBOARD_PROGRAM);
	if (keyboard != NO_PROCESS) {
		cock::user::cap_grant(keyboard, CapabilityType::IO_PORTS, PS2_DATA_PORT,
							  SINGLE_RESOURCE);
		cock::user::cap_grant(keyboard, CapabilityType::IRQ, KEYBOARD_IRQ_LINE,
							  SINGLE_RESOURCE);
		announce(keyboard, display);
	}

	spawnProgram(client, LAST_PROGRAM);
	cock::user::exit_process(EXIT_SUCCESS_STATUS);
}
