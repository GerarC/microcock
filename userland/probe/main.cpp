#include <cock/abi/capability.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

using cock::abi::CapabilityType;
using cock::ipc::Message;
using cock::ipc::MessageType;

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
constexpr long WRITE_FAILED = -1;
constexpr size_t CHECK_COUNT = 22;
constexpr size_t OVERSIZED_MAPPING = 0x20000000;
constexpr size_t MAPPING_ATTEMPTS = 64;
constexpr size_t OVERSIZED_WRITE = 200;
constexpr uint32_t PING_VALUE = 0x5EED;
constexpr int MUNMAP_FAILED = -1;
constexpr char SAMPLE_OUTPUT[] = "probe: printf -42 4294967295 FF hi Z 0xBEEF\n";
constexpr char TERMINATOR = '\0';
constexpr size_t NEWLINE_LENGTH = 1;
constexpr size_t SMALL_ALLOCATION = 100;
constexpr size_t GROWN_ALLOCATION = 4000;
constexpr size_t ARRAY_LENGTH = 64;
constexpr size_t OVERFLOW_FACTOR = 2;
constexpr uint8_t FIRST_PATTERN = 0xA5;
constexpr uint8_t SECOND_PATTERN = 0x3C;
constexpr uint8_t ZERO_PATTERN = 0;
constexpr int SAMPLE_SIGNED = -42;
constexpr unsigned SAMPLE_UNSIGNED = 0xFFFFFFFF;
constexpr unsigned SAMPLE_HEX = 0xFF;
constexpr uintptr_t SAMPLE_POINTER = 0xBEEF;
constexpr size_t LONG_LINE_LENGTH = 300;
constexpr char LONG_LINE_FILL = '.';
constexpr uintptr_t EXIT_ALL_PASSED = 0;
constexpr uintptr_t EXIT_SOME_FAILED = 1;

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

bool refusesReservedMessage(MessageType type) {
	Message message = {};
	message.type = type;
	return !cock::user::ipc_send(cock::user::self(), message);
}

void *mapPage() {
	return mmap(nullptr, PAGE_BYTES, MAPPING_PROTECTION, MAP_ANONYMOUS, NO_FILE, 0);
}

bool mappingQuotaIsEnforced() {
	void *huge = mmap(nullptr, OVERSIZED_MAPPING, MAPPING_PROTECTION,
					  MAP_ANONYMOUS, NO_FILE, 0);
	if (huge != MAP_FAILED) return false;
	return mapPage() != MAP_FAILED;
}

bool releasedAddressIsReused() {
	void *first = mapPage();
	if (first == MAP_FAILED) return false;
	if (munmap(first, PAGE_BYTES) != 0) return false;

	void *second = mapPage();
	if (second == MAP_FAILED) return false;
	return second == first && munmap(second, PAGE_BYTES) == 0;
}

bool regionTableIsBounded() {
	void *regions[MAPPING_ATTEMPTS];
	size_t mapped = 0;

	while (mapped < MAPPING_ATTEMPTS) {
		void *region = mapPage();
		if (region == MAP_FAILED) break;
		regions[mapped++] = region;
	}
	bool bounded = mapped > 0 && mapped < MAPPING_ATTEMPTS;

	for (size_t idx = 0; idx < mapped; idx++)
		munmap(regions[idx], PAGE_BYTES);
	return bounded && mapPage() != MAP_FAILED;
}

bool oversizedWriteIsCut() {
	char spaces[OVERSIZED_WRITE];
	for (size_t idx = 0; idx < OVERSIZED_WRITE; idx++)
		spaces[idx] = ' ';

	uintptr_t written = syscall(SYS_WRITE, cock::user::STDOUT_FILE,
								reinterpret_cast<uintptr_t>(spaces), OVERSIZED_WRITE,
								0, 0, 0);
	return written > 0 && written < OVERSIZED_WRITE;
}

bool messageToSelfArrives() {
	uint32_t self = cock::user::self();

	Message sent = {};
	sent.type = MessageType::SYS_PING;
	sent.payload.args.arg0 = PING_VALUE;
	if (!cock::user::ipc_send(self, sent)) return false;

	Message received = {};
	if (!cock::user::ipc_recv(received)) return false;
	return received.type == MessageType::SYS_PING &&
		   received.sender_pid == self &&
		   received.payload.args.arg0 == PING_VALUE;
}

bool printfFormatsAndLongLines() {
	int formats = printf("probe: printf %d %u %x %s %c %p\n", SAMPLE_SIGNED,
						 SAMPLE_UNSIGNED, SAMPLE_HEX, "hi", 'Z',
						 reinterpret_cast<void *>(SAMPLE_POINTER));
	int formatsExpected = sizeof(SAMPLE_OUTPUT) - sizeof(TERMINATOR);
	char line[LONG_LINE_LENGTH + 1];
	for (size_t idx = 0; idx < LONG_LINE_LENGTH; idx++)
		line[idx] = LONG_LINE_FILL;
	line[LONG_LINE_LENGTH] = TERMINATOR;
	int longLine = printf("%s\n", line);
	return formats == formatsExpected &&
		   longLine == static_cast<int>(LONG_LINE_LENGTH + NEWLINE_LENGTH);
}

bool holdsPattern(const void *memory, size_t length, uint8_t pattern) {
	const uint8_t *bytes = static_cast<const uint8_t *>(memory);
	for (size_t idx = 0; idx < length; idx++)
		if (bytes[idx] != pattern) return false;
	return true;
}

bool mallocReusesAndCoalesces() {
	uint8_t *first = static_cast<uint8_t *>(malloc(SMALL_ALLOCATION));
	uint8_t *second = static_cast<uint8_t *>(malloc(SMALL_ALLOCATION));
	if (!first || !second || first == second) return false;

	memset(first, FIRST_PATTERN, SMALL_ALLOCATION);
	memset(second, SECOND_PATTERN, SMALL_ALLOCATION);
	if (!holdsPattern(first, SMALL_ALLOCATION, FIRST_PATTERN)) return false;

	free(first);
	void *reused = malloc(SMALL_ALLOCATION);
	if (reused != first) return false;

	free(reused);
	free(second);
	void *merged = malloc(SMALL_ALLOCATION * OVERFLOW_FACTOR);
	bool coalesced = merged == first;
	free(merged);
	return coalesced;
}

bool reallocKeepsContents() {
	void *start = realloc(nullptr, SMALL_ALLOCATION);
	if (!start) return false;
	memset(start, FIRST_PATTERN, SMALL_ALLOCATION);

	void *grown = realloc(start, GROWN_ALLOCATION);
	if (!grown) return false;
	bool kept = holdsPattern(grown, SMALL_ALLOCATION, FIRST_PATTERN);
	free(grown);
	return kept;
}

bool callocZeroesRecycledMemory() {
	uint8_t *dirty = static_cast<uint8_t *>(malloc(SMALL_ALLOCATION));
	if (!dirty) return false;
	memset(dirty, FIRST_PATTERN, SMALL_ALLOCATION);
	free(dirty);

	void *clean = calloc(ARRAY_LENGTH, sizeof(uint8_t));
	if (!clean) return false;
	bool zeroed = holdsPattern(clean, ARRAY_LENGTH, ZERO_PATTERN);
	free(clean);
	return zeroed && calloc(SIZE_MAX, OVERFLOW_FACTOR) == nullptr;
}

bool hugeAllocationFailsCleanly() {
	if (malloc(OVERSIZED_MAPPING) != nullptr) return false;
	if (malloc(SIZE_MAX) != nullptr) return false;

	void *small = malloc(SMALL_ALLOCATION);
	free(small);
	return small != nullptr;
}

bool operatorNewRoundTrips() {
	uint32_t *values = new uint32_t[ARRAY_LENGTH];
	for (size_t idx = 0; idx < ARRAY_LENGTH; idx++)
		values[idx] = static_cast<uint32_t>(idx);
	bool intact = true;
	for (size_t idx = 0; idx < ARRAY_LENGTH; idx++)
		intact = intact && values[idx] == idx;
	delete[] values;

	uint32_t *single = new uint32_t(PING_VALUE);
	intact = intact && *single == PING_VALUE;
	delete single;
	return intact;
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
			MUNMAP_FAILED,
		refusesReservedMessage(MessageType::IRQ_NOTIFICATION),
		refusesReservedMessage(MessageType::CAPABILITY_GRANTED),
		messageToSelfArrives(),
		oversizedWriteIsCut(),
		mappingQuotaIsEnforced(),
		releasedAddressIsReused(),
		regionTableIsBounded(),
		printfFormatsAndLongLines(),
		mallocReusesAndCoalesces(),
		reallocKeepsContents(),
		callocZeroesRecycledMemory(),
		hugeAllocationFailsCleanly(),
		operatorNewRoundTrips()};

	unsigned passed = 0;
	for (size_t idx = 0; idx < CHECK_COUNT; idx++) {
		if (results[idx]) {
			passed++;
			continue;
		}
		printf("probe: check %u FAILED\n", static_cast<unsigned>(idx + 1));
	}

	printf("probe: %u/%u checks passed\n", passed,
		   static_cast<unsigned>(CHECK_COUNT));
	cock::user::exit_process(passed == CHECK_COUNT ? EXIT_ALL_PASSED
												: EXIT_SOME_FAILED);
}
