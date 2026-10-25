#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <cock/user/crt0.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr char EXPECTED_NAME[] = "execchild";
constexpr char EXPECTED_FIRST[] = "alpha";
constexpr char EXPECTED_SECOND[] = "beta";
constexpr uint32_t EXPECTED_ARGS = 4;
constexpr uint32_t NAME_INDEX = 0;
constexpr uint32_t FIRST_INDEX = 1;
constexpr uint32_t SECOND_INDEX = 2;
constexpr uint32_t PID_INDEX = 3;
constexpr uint32_t NO_HANDLE = 0;
constexpr uint32_t DECIMAL_BASE = 10;
constexpr uint32_t NO_BITS = 0;
constexpr uintptr_t EXIT_PASSED = 0;
constexpr uintptr_t EXIT_FAILED = 1;

bool parse(const char *text, uint32_t &value) {
	if (text == nullptr || *text == '\0') return false;
	value = 0;
	for (; *text != '\0'; text++) {
		if (*text < '0' || *text > '9') return false;
		value = value * DECIMAL_BASE + static_cast<uint32_t>(*text - '0');
	}
	return true;
}

bool argumentsArrived() {
	using cock::user::argument;
	return cock::user::argumentCount() == EXPECTED_ARGS && strcmp(argument(NAME_INDEX), EXPECTED_NAME) == 0 &&
		   strcmp(argument(FIRST_INDEX), EXPECTED_FIRST) == 0 &&
		   strcmp(argument(SECOND_INDEX), EXPECTED_SECOND) == 0;
}

bool sameProcess() {
	uint32_t before = 0;
	return parse(cock::user::argument(PID_INDEX), before) && before == cock::user::self();
}

bool otherThreadsGone() {
	uint32_t mask = cock::user::process_state_mask(cock::user::self());
	return (mask & cock::abi::process::THREAD_BLOCKED) == NO_BITS;
}

} // namespace

extern "C" int main() {
	bool arguments = argumentsArrived();
	bool same = sameProcess();
	bool handle = cock::user::initialHandle() != NO_HANDLE;
	bool alone = otherThreadsGone();
	if (!arguments || !same || !handle || !alone) {
		printf("execchild: wrong, arguments %d pid %d handle %d alone %d\n", static_cast<int>(arguments),
			   static_cast<int>(same), static_cast<int>(handle), static_cast<int>(alone));
		return EXIT_FAILED;
	}

	puts("execchild: ok");
	return EXIT_PASSED;
}
