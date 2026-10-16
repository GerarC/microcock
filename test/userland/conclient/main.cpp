#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

namespace {

constexpr int STDIN_DESCRIPTOR = 0;
constexpr size_t LINE_CAPACITY = 128;
constexpr char NEWLINE = '\n';
constexpr char TERMINATOR = '\0';
constexpr uintptr_t EXIT_OK = 0;
constexpr uintptr_t EXIT_FAILED = 1;

}

extern "C" void _start() {
	printf("conclient: ready\n");

	char line[LINE_CAPACITY] = {};
	size_t length = 0;
	bool complete = false;
	while (!complete) {
		char chunk[LINE_CAPACITY] = {};
		ssize_t count = read(STDIN_DESCRIPTOR, chunk, sizeof(chunk));
		if (count <= 0) cock::user::exit_process(EXIT_FAILED);

		for (ssize_t index = 0; index < count && !complete; index++) {
			if (chunk[index] == NEWLINE) {
				complete = true;
			} else if (length + 1 < LINE_CAPACITY) {
				line[length++] = chunk[index];
			}
		}
	}
	line[length] = TERMINATOR;

	printf("conclient: line %s\n", line);
	printf("conclient: ok\n");
	cock::user::exit_process(EXIT_OK);
}
