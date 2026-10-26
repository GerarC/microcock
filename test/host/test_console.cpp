#include <cock/abi/console.hpp>
#include <cock/abi/console_history.hpp>
#include <cock/abi/console_line.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using namespace cock::abi::console;

namespace {

constexpr size_t TAKE_LIMIT = 8;
constexpr size_t NOTHING = 0;
constexpr size_t FIRST_INDEX = 0;
constexpr size_t SECOND_INDEX = 1;
constexpr size_t THIRD_INDEX = 2;
constexpr size_t FOURTH_INDEX = 3;
constexpr size_t THREE_KEYS_AND_NEWLINE = 4;
constexpr size_t TWO_KEYS_AND_NEWLINE = 3;
constexpr size_t SMALL_STEP = 2;
constexpr char FIRST_KEY = 'a';
constexpr char SECOND_KEY = 'b';
constexpr char THIRD_KEY = 'c';
constexpr char CONTROL_KEY = '\x01';
constexpr char THREE_KEYS[] = "abc";
constexpr char SAMPLE_BYTES[] = "hello console";
constexpr size_t SAMPLE_LENGTH = sizeof(SAMPLE_BYTES) - 1;
constexpr size_t SPARE_BYTE = 1;
constexpr uint32_t WRITE_CODE = static_cast<uint32_t>(Op::WRITE);
constexpr char FIRST_COMMAND[] = "ls";
constexpr size_t FIRST_COMMAND_LENGTH = sizeof(FIRST_COMMAND) - 1;
constexpr char SECOND_COMMAND[] = "echo hi";
constexpr char DRAFT_TEXT[] = "pw";
constexpr size_t DRAFT_LENGTH = sizeof(DRAFT_TEXT) - 1;
constexpr char FIRST_KEY_TEXT[] = "a";
constexpr uint32_t UNKNOWN_SPECIAL_KEY = SPECIAL_KEY_BASE + 0x80;

void typeLine(LineDiscipline &discipline, const char *text) {
	for (size_t index = 0; text[index] != '\0'; index++)
		discipline.feed(text[index]);
}

void checkEcho() {
	LineDiscipline discipline;
	CHECK_TRUE(discipline.feed(FIRST_KEY) == Echo::CHARACTER);
	CHECK_TRUE(discipline.feed(CHAR_BACKSPACE) == Echo::ERASE);
	CHECK_TRUE(discipline.feed(CHAR_BACKSPACE) == Echo::NONE);
	CHECK_TRUE(discipline.feed(CONTROL_KEY) == Echo::NONE);
	CHECK_TRUE(discipline.feed(CHAR_NEWLINE) == Echo::NEWLINE);
}

void checkLineIsHeldUntilNewline() {
	LineDiscipline discipline;
	char out[TAKE_LIMIT] = {};
	typeLine(discipline, THREE_KEYS);
	CHECK_FALSE(discipline.hasInput());
	CHECK_EQ(NOTHING, discipline.take(out, TAKE_LIMIT));

	discipline.feed(CHAR_NEWLINE);
	CHECK_TRUE(discipline.hasInput());
	CHECK_EQ(THREE_KEYS_AND_NEWLINE, discipline.take(out, TAKE_LIMIT));
	CHECK_EQ(FIRST_KEY, out[FIRST_INDEX]);
	CHECK_EQ(SECOND_KEY, out[SECOND_INDEX]);
	CHECK_EQ(THIRD_KEY, out[THIRD_INDEX]);
	CHECK_EQ(CHAR_NEWLINE, out[FOURTH_INDEX]);
	CHECK_FALSE(discipline.hasInput());
}

void checkBackspaceEditsTheLine() {
	LineDiscipline discipline;
	char out[TAKE_LIMIT] = {};
	discipline.feed(FIRST_KEY);
	discipline.feed(SECOND_KEY);
	discipline.feed(CHAR_BACKSPACE);
	discipline.feed(THIRD_KEY);
	discipline.feed(CHAR_NEWLINE);
	CHECK_EQ(TWO_KEYS_AND_NEWLINE, discipline.take(out, TAKE_LIMIT));
	CHECK_EQ(FIRST_KEY, out[FIRST_INDEX]);
	CHECK_EQ(THIRD_KEY, out[SECOND_INDEX]);
}

void checkTakeInSteps() {
	LineDiscipline discipline;
	char out[TAKE_LIMIT] = {};
	typeLine(discipline, THREE_KEYS);
	discipline.feed(CHAR_NEWLINE);
	CHECK_EQ(SMALL_STEP, discipline.take(out, SMALL_STEP));
	CHECK_EQ(SECOND_KEY, out[SECOND_INDEX]);
	CHECK_EQ(SMALL_STEP, discipline.take(out, TAKE_LIMIT));
	CHECK_EQ(THIRD_KEY, out[FIRST_INDEX]);
}

void checkLineLimit() {
	LineDiscipline discipline;
	size_t echoed = 0;
	for (size_t index = 0; index < LINE_CAPACITY + TAKE_LIMIT; index++) {
		if (discipline.feed(FIRST_KEY) == Echo::CHARACTER) echoed++;
	}
	CHECK_EQ(LINE_CAPACITY, echoed);
}

void runLine(LineDiscipline &discipline, const char *text) {
	typeLine(discipline, text);
	discipline.feed(CHAR_NEWLINE);
}

bool currentLineIs(const LineDiscipline &discipline, const char *text) {
	size_t length = 0;
	while (text[length] != '\0')
		length++;
	if (discipline.currentLength() != length) return false;
	for (size_t index = 0; index < length; index++) {
		if (discipline.currentLine()[index] != text[index]) return false;
	}
	return true;
}

void checkHistoryRecall() {
	LineDiscipline discipline;
	CHECK_TRUE(discipline.feed(KEY_UP) == Echo::NONE);
	CHECK_TRUE(discipline.feed(UNKNOWN_SPECIAL_KEY) == Echo::NONE);

	runLine(discipline, FIRST_COMMAND);
	runLine(discipline, SECOND_COMMAND);
	typeLine(discipline, DRAFT_TEXT);

	CHECK_TRUE(discipline.feed(KEY_UP) == Echo::REPLACE);
	CHECK_EQ(DRAFT_LENGTH, discipline.replacedLength());
	CHECK_TRUE(currentLineIs(discipline, SECOND_COMMAND));
	CHECK_TRUE(discipline.feed(KEY_UP) == Echo::REPLACE);
	CHECK_TRUE(currentLineIs(discipline, FIRST_COMMAND));
	CHECK_TRUE(discipline.feed(KEY_UP) == Echo::NONE);
	CHECK_TRUE(currentLineIs(discipline, FIRST_COMMAND));

	CHECK_TRUE(discipline.feed(KEY_DOWN) == Echo::REPLACE);
	CHECK_TRUE(currentLineIs(discipline, SECOND_COMMAND));
	CHECK_TRUE(discipline.feed(KEY_DOWN) == Echo::REPLACE);
	CHECK_TRUE(currentLineIs(discipline, DRAFT_TEXT));
	CHECK_TRUE(discipline.feed(KEY_DOWN) == Echo::NONE);
}

void checkRecalledLineRuns() {
	LineDiscipline discipline;
	char out[TAKE_LIMIT] = {};
	runLine(discipline, FIRST_KEY_TEXT);
	discipline.take(out, TAKE_LIMIT);

	discipline.feed(KEY_UP);
	discipline.feed(CHAR_NEWLINE);
	CHECK_EQ(TWO_KEYS_AND_NEWLINE - SPARE_BYTE, discipline.take(out, TAKE_LIMIT));
	CHECK_EQ(FIRST_KEY, out[FIRST_INDEX]);
	CHECK_EQ(CHAR_NEWLINE, out[SECOND_INDEX]);
}

void checkHistoryKeepsDistinctLines() {
	History history;
	history.add(FIRST_COMMAND, FIRST_COMMAND_LENGTH);
	history.add(FIRST_COMMAND, FIRST_COMMAND_LENGTH);
	history.add(FIRST_COMMAND, NOTHING);
	CHECK_EQ(SPARE_BYTE, history.count());

	for (size_t index = 0; index < HISTORY_CAPACITY + SMALL_STEP; index++) {
		char text[SMALL_STEP] = {static_cast<char>(FIRST_KEY + static_cast<char>(index)), '\0'};
		history.add(text, SPARE_BYTE);
	}
	CHECK_EQ(HISTORY_CAPACITY, history.count());
	CHECK_EQ(static_cast<char>(FIRST_KEY + static_cast<char>(SMALL_STEP)), history.line(FIRST_INDEX)[FIRST_INDEX]);
	CHECK_TRUE(history.line(HISTORY_CAPACITY) == nullptr);
}

void checkPacking() {
	uint32_t words[FIRST_DATA_WORD + DATA_WORDS] = {};
	words[HEADER_WORD] = pack(WRITE_CODE, CHUNK_BYTES);
	CHECK_EQ(WRITE_CODE, codeOf(words[HEADER_WORD]));
	CHECK_EQ(CHUNK_BYTES, lengthOf(words[HEADER_WORD]));
	CHECK_FALSE(isLast(words[HEADER_WORD]));

	uint32_t closing = pack(WRITE_CODE, CHUNK_BYTES, true);
	CHECK_TRUE(isLast(closing));
	CHECK_EQ(WRITE_CODE, codeOf(closing));
	CHECK_EQ(CHUNK_BYTES, lengthOf(closing));

	packBytes(words, SAMPLE_BYTES, CHUNK_BYTES);
	char out[CHUNK_BYTES + SPARE_BYTE] = {};
	unpackBytes(words, out, CHUNK_BYTES);
	bool same = true;
	for (size_t index = 0; index < CHUNK_BYTES; index++)
		same = same && out[index] == SAMPLE_BYTES[index];
	CHECK_TRUE(same);
	CHECK_TRUE(SAMPLE_LENGTH > CHUNK_BYTES);
}

} // namespace

void testConsole() {
	checkEcho();
	checkLineIsHeldUntilNewline();
	checkBackspaceEditsTheLine();
	checkTakeInSteps();
	checkLineLimit();
	checkHistoryRecall();
	checkRecalledLineRuns();
	checkHistoryKeepsDistinctLines();
	checkPacking();
}

} // namespace host_test
