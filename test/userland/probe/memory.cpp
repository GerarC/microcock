#include "probe.hpp"
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/syscalls.h>

namespace probe {

namespace {

constexpr uint8_t TEST_PATTERN = 0x5A;
constexpr size_t MAPPING_ATTEMPTS = 64;

void *mapPage() {
	return mmap(nullptr, PAGE_BYTES, MAPPING_PROTECTION, MAP_ANONYMOUS, NO_FILE, 0);
}

constexpr int NO_PROTECTION = 0;
constexpr int UNKNOWN_PROTECTION = 0x08;
constexpr uintptr_t STDIN_DESCRIPTOR = 0;
constexpr uintptr_t PROBE_LENGTH = 1;
constexpr uintptr_t END_OF_INPUT = 0;

bool kernelMayWriteInto(void *memory) {
	return syscall(SYS_READ, STDIN_DESCRIPTOR, reinterpret_cast<uintptr_t>(memory),
				   PROBE_LENGTH, 0, 0, 0) == END_OF_INPUT;
}

} // namespace

bool mappingProtectionIsHonored() {
	void *readOnly = mmap(nullptr, PAGE_BYTES, PROT_READ, MAP_ANONYMOUS, NO_FILE, 0);
	if (readOnly == MAP_FAILED) return false;
	bool zeroed = *static_cast<volatile uint8_t *>(readOnly) == 0;
	bool refused = !kernelMayWriteInto(readOnly);
	bool released = munmap(readOnly, PAGE_BYTES) == 0;

	void *writable = mapPage();
	if (writable == MAP_FAILED) return false;
	bool accepted = kernelMayWriteInto(writable);
	munmap(writable, PAGE_BYTES);

	return zeroed && refused && released && accepted &&
		   mmap(nullptr, PAGE_BYTES, NO_PROTECTION, MAP_ANONYMOUS, NO_FILE, 0) ==
			   MAP_FAILED &&
		   mmap(nullptr, PAGE_BYTES, UNKNOWN_PROTECTION, MAP_ANONYMOUS, NO_FILE, 0) ==
			   MAP_FAILED;
}

bool anonymousMemoryWorks() {
	void *memory = mapPage();
	if (memory == MAP_FAILED) return false;

	volatile uint8_t *bytes = static_cast<volatile uint8_t *>(memory);
	bool zeroed = bytes[0] == 0 && bytes[PAGE_BYTES - 1] == 0;
	bytes[0] = TEST_PATTERN;
	bool kept = bytes[0] == TEST_PATTERN;

	bool released = munmap(memory, PAGE_BYTES) == 0;
	bool releasedTwice = munmap(memory, PAGE_BYTES) == 0;
	return zeroed && kept && released && !releasedTwice;
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

} // namespace probe
