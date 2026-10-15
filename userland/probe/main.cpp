#include <cock/abi/capability.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

using cock::abi::CapabilityType;

namespace {

constexpr uint16_t PS2_DATA_PORT = 0x60;
constexpr uint32_t SINGLE_RESOURCE = 1;
constexpr uint32_t KEYBOARD_IRQ_LINE = 1;
constexpr uint32_t ROOT_SERVER_PID = 1;
constexpr uintptr_t VIDEO_PHYSICAL = 0xB8000;
constexpr uintptr_t RAM_PHYSICAL = 0x100000;
constexpr uintptr_t MAP_TARGET = 0x80000000;
constexpr uintptr_t KERNEL_ADDRESS = 0xC0100000;
constexpr uintptr_t UNMAPPED_USER_ADDRESS = 0x30000000;
constexpr size_t PAGE_BYTES = 0x1000;
constexpr size_t PROBE_LENGTH = 16;
constexpr int NO_FILE = -1;
constexpr int MAPPING_PROTECTION = PROT_READ | PROT_WRITE;
constexpr uint8_t TEST_PATTERN = 0x5A;
constexpr unsigned DECIMAL_BASE = 10;
constexpr size_t NUMBER_BUFFER_SIZE = 12;
constexpr long WRITE_FAILED = -1;
constexpr size_t CHECK_COUNT = 9;
constexpr int MUNMAP_FAILED = -1;
constexpr uintptr_t EXIT_ALL_PASSED = 0;
constexpr uintptr_t EXIT_SOME_FAILED = 1;

void print(const char *text) {
	size_t length = 0;
	while (text[length] != '\0')
		length++;
	cock::user::write_text(text, length);
}

void printNumber(unsigned value) {
	char digits[NUMBER_BUFFER_SIZE];
	size_t position = NUMBER_BUFFER_SIZE;
	do {
		digits[--position] = static_cast<char>('0' + value % DECIMAL_BASE);
		value /= DECIMAL_BASE;
	} while (value != 0 && position > 0);
	cock::user::write_text(&digits[position], NUMBER_BUFFER_SIZE - position);
}

bool anonymousMemoryWorks() {
	void *memory = mmap(nullptr, PAGE_BYTES, MAPPING_PROTECTION, MAP_ANONYMOUS,
						NO_FILE, 0);
	if (memory == MAP_FAILED) return false;

	volatile uint8_t *bytes = static_cast<volatile uint8_t *>(memory);
	bool zeroed = bytes[0] == 0 && bytes[PAGE_BYTES - 1] == 0;
	bytes[0] = TEST_PATTERN;
	bool kept = bytes[0] == TEST_PATTERN;

	bool released = munmap(memory, PAGE_BYTES) == 0;
	bool releasedTwice = munmap(memory, PAGE_BYTES) == 0;
	return zeroed && kept && released && !releasedTwice;
}

} // namespace

extern "C" void _start() {
	bool results[CHECK_COUNT] = {
		!cock::user::request_ports(PS2_DATA_PORT, SINGLE_RESOURCE),
		!cock::user::irq_bind(KEYBOARD_IRQ_LINE),
		!cock::user::cap_grant(ROOT_SERVER_PID, CapabilityType::IO_PORTS,
							   PS2_DATA_PORT, SINGLE_RESOURCE),
		mmap(reinterpret_cast<void *>(MAP_TARGET), PAGE_BYTES,
			 MAPPING_PROTECTION, MAP_PHYS | MAP_FIXED, NO_FILE,
			 VIDEO_PHYSICAL) == MAP_FAILED,
		mmap(reinterpret_cast<void *>(MAP_TARGET), PAGE_BYTES,
			 MAPPING_PROTECTION, MAP_PHYS | MAP_FIXED, NO_FILE,
			 RAM_PHYSICAL) == MAP_FAILED,
		cock::user::write_text(reinterpret_cast<const char *>(KERNEL_ADDRESS),
							   PROBE_LENGTH) == WRITE_FAILED,
		cock::user::write_text(
			reinterpret_cast<const char *>(UNMAPPED_USER_ADDRESS),
			PROBE_LENGTH) == WRITE_FAILED,
		anonymousMemoryWorks(),
		munmap(reinterpret_cast<void *>(KERNEL_ADDRESS), PAGE_BYTES) ==
			MUNMAP_FAILED};

	unsigned passed = 0;
	for (size_t idx = 0; idx < CHECK_COUNT; idx++) {
		if (results[idx]) {
			passed++;
			continue;
		}
		print("probe: check ");
		printNumber(static_cast<unsigned>(idx + 1));
		print(" FAILED\n");
	}

	print("probe: ");
	printNumber(passed);
	print("/");
	printNumber(CHECK_COUNT);
	print(" checks passed\n");
	cock::user::exit_process(passed == CHECK_COUNT ? EXIT_ALL_PASSED
												: EXIT_SOME_FAILED);
}
