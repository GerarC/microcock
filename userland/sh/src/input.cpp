#include <cock/shell/words.hpp>
#include <cock/user/api.hpp>
#include <sh/arguments.hpp>
#include <sh/input.hpp>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

namespace cock::sh {

namespace {

constexpr int STDIN_DESCRIPTOR = 0;
constexpr size_t READ_CHUNK_BYTES = 12;
constexpr uint32_t RETRY_MS = 100;
constexpr size_t NO_LENGTH = 0;

} // namespace

void readLine(char *line) {
	size_t length = NO_LENGTH;
	bool complete = false;

	while (!complete) {
		char chunk[READ_CHUNK_BYTES] = {};
		ssize_t count = read(STDIN_DESCRIPTOR, chunk, sizeof(chunk));
		if (count <= 0) {
			cock::user::sleep_ms(RETRY_MS);
			continue;
		}

		for (ssize_t index = 0; index < count && !complete; index++) {
			if (chunk[index] == NEWLINE) {
				complete = true;
			} else if (length + 1 < cock::shell::MAX_LINE_BYTES) {
				line[length++] = chunk[index];
			}
		}
	}
	line[length] = cock::shell::TERMINATOR;
}

} // namespace cock::sh
