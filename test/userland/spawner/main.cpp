#include <cock/abi/service.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

namespace {

constexpr char PROGRAM_PATH[] = "/bin/hello.elf";
constexpr char CHILD_NAME[] = "spawned";
constexpr size_t IMAGE_BYTES = 0x8000;
constexpr uint32_t FORGED_LABEL = 7;
constexpr uint32_t NO_LABELS = 0;
constexpr int EXIT_SERVED = 0;
constexpr int EXIT_REFUSED = 1;

uint8_t image[IMAGE_BYTES];

size_t loadImage() {
	int fd = open(PROGRAM_PATH, O_RDONLY);
	if (fd < 0) return 0;

	size_t total = 0;
	for (;;) {
		ssize_t step = read(fd, image + total, IMAGE_BYTES - total);
		if (step <= 0) break;
		total += static_cast<size_t>(step);
	}
	close(fd);
	return total;
}

void waitForGrant() {
	cock::ipc::Message message = {};
	do {
		if (!cock::user::ipc_recv(message)) return;
	} while (message.type != cock::ipc::MessageType::CAPABILITY_GRANTED);
}

bool refusesForgedLabel(size_t size) {
	cock::abi::service::SpawnRights rights = {};
	rights.label = FORGED_LABEL;
	rights.ipcCount = NO_LABELS;
	return cock::user::spawn(image, size, CHILD_NAME, false, &rights) == cock::user::NO_PROCESS;
}

} // namespace

extern "C" int main() {
	waitForGrant();
	size_t size = loadImage();
	bool served = size != 0;
	served = served && refusesForgedLabel(size);
	if (served) puts("spawner: forged label refused");
	served = served && cock::user::spawn(image, size, CHILD_NAME) != cock::user::NO_PROCESS;
	if (served) puts("spawner: spawned a program");

	puts(served ? "spawner: ok" : "spawner: refused");
	_exit(served ? EXIT_SERVED : EXIT_REFUSED);
}
