#include <cock/abi/console.hpp>
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

void checkPacking() {
	uint32_t words[FIRST_DATA_WORD + DATA_WORDS] = {};
	words[HEADER_WORD] = pack(WRITE_CODE, CHUNK_BYTES);
	CHECK_EQ(WRITE_CODE, codeOf(words[HEADER_WORD]));
	CHECK_EQ(CHUNK_BYTES, lengthOf(words[HEADER_WORD]));

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
	checkPacking();
}

} // namespace host_test
