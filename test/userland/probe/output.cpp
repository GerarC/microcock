#include "probe.hpp"
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace probe {

namespace {

constexpr size_t OVERSIZED_WRITE = 200;
constexpr char SAMPLE_OUTPUT[] = "probe: printf -42 4294967295 FF hi Z 0xBEEF\n";
constexpr char TERMINATOR = '\0';
constexpr size_t NEWLINE_LENGTH = 1;
constexpr int SAMPLE_SIGNED = -42;
constexpr unsigned SAMPLE_UNSIGNED = 0xFFFFFFFF;
constexpr unsigned SAMPLE_HEX = 0xFF;
constexpr uintptr_t SAMPLE_POINTER = 0xBEEF;
constexpr size_t LONG_LINE_LENGTH = 300;
constexpr char LONG_LINE_FILL = '.';

} // namespace

bool oversizedWriteIsCut() {
	char spaces[OVERSIZED_WRITE];
	for (size_t idx = 0; idx < OVERSIZED_WRITE; idx++)
		spaces[idx] = ' ';

	uintptr_t written = syscall(SYS_WRITE, cock::user::STDOUT_FILE,
								reinterpret_cast<uintptr_t>(spaces), OVERSIZED_WRITE,
								0, 0, 0);
	return written > 0 && written < OVERSIZED_WRITE;
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

} // namespace probe
