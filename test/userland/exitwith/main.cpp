#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr uint32_t STATUS_INDEX = 1;
constexpr uint32_t DECIMAL_BASE = 10;
constexpr int EXIT_NO_STATUS = 0;

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

extern "C" int main(int argc, char **argv) {
	printf("exitwith: %d arguments, first %s\n", argc, argv[0]);
	int status = EXIT_NO_STATUS;
	if (argc > static_cast<int>(STATUS_INDEX)) parse(argv[STATUS_INDEX], status);
	return status;
}
