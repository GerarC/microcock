#include <cock/user/api.hpp>
#include <cock/user/crt0.hpp>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr char REPORT[] = "exec ok";
constexpr size_t REPORT_BYTES = sizeof(REPORT) - sizeof(char);
constexpr uint32_t ARGUMENT_COUNT = 4;
constexpr uint32_t PIPE_INDEX = 1;
constexpr uint32_t FILE_INDEX = 2;
constexpr uint32_t EXPECTED_INDEX = 3;
constexpr uint32_t DECIMAL_BASE = 10;
constexpr size_t BUFFER_BYTES = 32;
constexpr int EXIT_PASSED = 0;
constexpr int EXIT_BAD_ARGUMENTS = 1;
constexpr int EXIT_FILE_WRONG = 2;
constexpr int EXIT_PIPE_WRONG = 3;
constexpr int NOT_PARSED = -1;

bool parse(const char *text, int &value) {
	if (text == nullptr || *text == '\0') return false;
	value = 0;
	for (; *text != '\0'; text++) {
		if (*text < '0' || *text > '9') return false;
		value = value * static_cast<int>(DECIMAL_BASE) + (*text - '0');
	}
	return true;
}

}

extern "C" int main() {
	int pipeEnd = NOT_PARSED;
	int fileEnd = NOT_PARSED;
	const char *expected = cock::user::argument(EXPECTED_INDEX);
	if (cock::user::argumentCount() != ARGUMENT_COUNT || !parse(cock::user::argument(PIPE_INDEX), pipeEnd) ||
		!parse(cock::user::argument(FILE_INDEX), fileEnd) || expected == nullptr)
		return EXIT_BAD_ARGUMENTS;

	char buffer[BUFFER_BYTES] = {};
	size_t length = strlen(expected);
	if (read(fileEnd, buffer, length) != static_cast<ssize_t>(length) || memcmp(buffer, expected, length) != 0)
		return EXIT_FILE_WRONG;
	if (write(pipeEnd, REPORT, REPORT_BYTES) != static_cast<ssize_t>(REPORT_BYTES)) return EXIT_PIPE_WRONG;

	puts("descchild: ok");
	return EXIT_PASSED;
}
