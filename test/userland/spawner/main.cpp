#include <cock/abi/executable.hpp>
#include <cock/abi/process.hpp>
#include <cock/abi/service.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/spawn.hpp>
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
constexpr uintptr_t PROCESS_LIMIT = cock::abi::executable::USER_SPACE_LIMIT;
constexpr uintptr_t PAGE_BYTES = cock::abi::process::PENDING_PAGE_BYTES;
constexpr uintptr_t IMAGE_ADDRESS = 0x400000;
constexpr uintptr_t SECOND_IMAGE_ADDRESS = 0x1000000;
constexpr uintptr_t STACK_ADDRESS = PROCESS_LIMIT - cock::abi::process::PENDING_STACK_BYTES;
constexpr uintptr_t FIRST_PAGE_ADDRESS = 0;
constexpr uintptr_t MISALIGNED_ADDRESS = IMAGE_ADDRESS + 1;
constexpr uintptr_t WRAPPING_ADDRESS = 0xFFFFF000;
constexpr size_t NO_PAGES = 0;
constexpr size_t ONE_PAGE = 1;
constexpr size_t TWO_PAGES = 2;
constexpr size_t ONE_HANDLE = 1;
constexpr size_t ONE_BYTE = 1;
constexpr size_t TWO_BYTES = 2;
constexpr size_t ALLOWED_PAGES = cock::abi::process::MAX_PENDING_IMAGE_BYTES / PAGE_BYTES;
constexpr size_t OVERSIZED_PAGES = ALLOWED_PAGES + 1;

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

uint32_t createPending() {
	return cock::user::proc_create(CHILD_NAME, false, nullptr, PROCESS_LIMIT);
}

bool refusesBadRanges(uint32_t handle) {
	return !cock::user::proc_map(handle, FIRST_PAGE_ADDRESS, ONE_PAGE) &&
		   !cock::user::proc_map(handle, MISALIGNED_ADDRESS, ONE_PAGE) &&
		   !cock::user::proc_map(handle, STACK_ADDRESS, ONE_PAGE) &&
		   !cock::user::proc_map(handle, WRAPPING_ADDRESS, TWO_PAGES) &&
		   !cock::user::proc_map(handle, IMAGE_ADDRESS, NO_PAGES) &&
		   !cock::user::proc_map(handle, IMAGE_ADDRESS, OVERSIZED_PAGES) &&
		   !cock::user::proc_write(handle, IMAGE_ADDRESS, image, ONE_BYTE) &&
		   !cock::user::proc_protect(handle, IMAGE_ADDRESS, ONE_PAGE) &&
		   cock::user::proc_start(handle, IMAGE_ADDRESS) == cock::user::NO_PROCESS;
}

bool refusesBadOrder(uint32_t handle) {
	return cock::user::proc_map(handle, IMAGE_ADDRESS, ONE_PAGE) &&
		   !cock::user::proc_map(handle, IMAGE_ADDRESS, ONE_PAGE) &&
		   cock::user::proc_write(handle, IMAGE_ADDRESS, image, ONE_BYTE) &&
		   !cock::user::proc_write(handle, IMAGE_ADDRESS + PAGE_BYTES - ONE_BYTE, image, TWO_BYTES) &&
		   !cock::user::proc_write(handle, IMAGE_ADDRESS, nullptr, ONE_BYTE) &&
		   cock::user::proc_protect(handle, IMAGE_ADDRESS, ONE_PAGE) &&
		   !cock::user::proc_write(handle, IMAGE_ADDRESS, image, ONE_BYTE) &&
		   cock::user::proc_start(handle, SECOND_IMAGE_ADDRESS) == cock::user::NO_PROCESS &&
		   cock::user::proc_start(handle, STACK_ADDRESS) == cock::user::NO_PROCESS;
}

bool refusesTooMuchMemory(uint32_t handle) {
	return cock::user::proc_map(handle, IMAGE_ADDRESS, ALLOWED_PAGES) &&
		   !cock::user::proc_map(handle, SECOND_IMAGE_ADDRESS + ALLOWED_PAGES * PAGE_BYTES, ONE_PAGE);
}

bool refusesTooManyPending() {
	uint32_t handles[cock::abi::process::MAX_PENDING_PER_PROCESS + ONE_HANDLE];
	size_t created = 0;
	for (; created < sizeof(handles) / sizeof(handles[0]); created++) {
		handles[created] = createPending();
		if (handles[created] == cock::abi::process::NO_PENDING_PROCESS) break;
	}
	bool refused = created == cock::abi::process::MAX_PENDING_PER_PROCESS;
	for (size_t index = 0; index < created; index++)
		refused = cock::user::proc_discard(handles[index]) && refused;
	return refused;
}

bool refusesLoaderMistakes() {
	uint32_t handle = createPending();
	if (handle == cock::abi::process::NO_PENDING_PROCESS) return false;

	bool refused = refusesBadRanges(handle) && refusesBadOrder(handle);
	refused = cock::user::proc_discard(handle) && refused;
	refused = !cock::user::proc_discard(handle) && !cock::user::proc_map(handle, IMAGE_ADDRESS, ONE_PAGE) && refused;

	handle = createPending();
	if (handle == cock::abi::process::NO_PENDING_PROCESS) return false;
	refused = refusesTooMuchMemory(handle) && refused;
	refused = cock::user::proc_discard(handle) && refused;
	return refusesTooManyPending() && refused;
}

} // namespace

extern "C" int main() {
	waitForGrant();
	size_t size = loadImage();
	bool served = size != 0;
	served = served && refusesForgedLabel(size);
	if (served) puts("spawner: forged label refused");
	served = served && refusesLoaderMistakes();
	if (served) puts("spawner: loader mistakes refused");
	served = served && cock::user::spawn(image, size, CHILD_NAME) != cock::user::NO_PROCESS;
	if (served) puts("spawner: spawned a program");

	puts(served ? "spawner: ok" : "spawner: refused");
	_exit(served ? EXIT_SERVED : EXIT_REFUSED);
}
