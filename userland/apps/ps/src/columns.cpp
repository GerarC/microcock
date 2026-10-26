#include <cock/abi/pm.hpp>
#include <ps/columns.hpp>
#include <ps/palette.hpp>
#include <ps/state.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::pm::NO_PID;
using cock::abi::pm::ProcessRecord;
using cock::abi::pm::State;

namespace cock::ps {

namespace {

constexpr size_t PID_WIDTH = 4;
constexpr size_t PARENT_WIDTH = 4;
constexpr size_t STATE_WIDTH = 4;
constexpr size_t STATE_LETTER_WIDTH = 1;
constexpr size_t ONE_DIGIT = 1;
constexpr uint32_t DECIMAL_RADIX = 10;
constexpr size_t FIRST_CHARACTER = 0;
constexpr char NO_CHARACTER = '\0';
constexpr char SEPARATOR = ' ';
constexpr char MISSING[] = "-";
constexpr char PID_TITLE[] = "PID";
constexpr char PARENT_TITLE[] = "PPID";
constexpr char STATE_TITLE[] = "STAT";
constexpr char NAME_TITLE[] = "COMMAND";

void spaces(size_t count) {
	for (size_t index = 0; index < count; index++) putchar(SEPARATOR);
}

size_t digitsOf(uint32_t value) {
	size_t digits = ONE_DIGIT;
	for (; value >= DECIMAL_RADIX; value /= DECIMAL_RADIX) digits++;
	return digits;
}

void rightAligned(const char *text, size_t width) {
	size_t used = strlen(text);
	spaces(width > used ? width - used : 0);
	printf("%s", text);
}

void number(uint32_t value, size_t width) {
	size_t used = digitsOf(value);
	spaces(width > used ? width - used : 0);
	printf("%u", static_cast<unsigned>(value));
}

bool hasExited(State state) {
	return state == State::EXITED;
}

} // namespace

void printHeader() {
	printf("%s", HEADER);
	rightAligned(PID_TITLE, PID_WIDTH);
	putchar(SEPARATOR);
	rightAligned(PARENT_TITLE, PARENT_WIDTH);
	putchar(SEPARATOR);
	printf("%s", STATE_TITLE);
	putchar(SEPARATOR);
	printf("%s%s\n", NAME_TITLE, RESET);
}

void printRow(const ProcessRecord &record) {
	printf("%s", NUMBER);
	number(record.pid, PID_WIDTH);
	putchar(SEPARATOR);
	if (record.parent == NO_PID) {
		printf("%s", MUTED);
		rightAligned(MISSING, PARENT_WIDTH);
	} else {
		number(record.parent, PARENT_WIDTH);
	}
	putchar(SEPARATOR);

	printf("%s%c%s", stateColor(record.state), stateLetter(record.state), RESET);
	spaces(STATE_WIDTH - STATE_LETTER_WIDTH);
	putchar(SEPARATOR);

	bool named = record.name[FIRST_CHARACTER] != NO_CHARACTER;
	printf("%s%s%s\n", named && !hasExited(record.state) ? NAME : MUTED, named ? record.name : MISSING, RESET);
}

} // namespace cock::ps
