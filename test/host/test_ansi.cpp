#include <cock/abi/ansi.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using namespace cock::abi::ansi;

namespace {

constexpr char GREEN_SEQUENCE[] = "\x1B[32m";
constexpr char BOLD_RED_SEQUENCE[] = "\x1B[1;31m";
constexpr char BRIGHT_CYAN_SEQUENCE[] = "\x1B[96m";
constexpr char RESET_SEQUENCE[] = "\x1B[0m";
constexpr char EMPTY_RESET_SEQUENCE[] = "\x1B[m";
constexpr char BLUE_BACKGROUND_SEQUENCE[] = "\x1B[44m";
constexpr char CURSOR_SEQUENCE[] = "\x1B[2J";
constexpr char BROKEN_SEQUENCE[] = "\x1B[3\n";
constexpr char NOT_A_SEQUENCE[] = "\x1Bx";
constexpr char LONG_SEQUENCE[] = "\x1B[99999999999999m";
constexpr char PLAIN_TEXT[] = "cock> ";
constexpr size_t NOTHING = 0;
constexpr size_t FIRST_BYTE = 0;
constexpr uint8_t BRIGHT_CYAN = static_cast<uint8_t>(Color::CYAN) + NORMAL_COLOR_COUNT;

Result feedAll(SgrParser &parser, const char *text) {
	Result last = Result::PRINT;
	for (size_t index = 0; text[index] != '\0'; index++)
		last = parser.feed(text[index]);
	return last;
}

size_t countPrinted(SgrParser &parser, const char *text) {
	size_t printed = NOTHING;
	for (size_t index = 0; text[index] != '\0'; index++) {
		if (parser.feed(text[index]) == Result::PRINT) printed++;
	}
	return printed;
}

void checkPlainTextPasses() {
	SgrParser parser;
	CHECK_EQ(sizeof(PLAIN_TEXT) - 1, countPrinted(parser, PLAIN_TEXT));
	CHECK_EQ(DEFAULT_COLOR, parser.style().foreground);
	CHECK_EQ(DEFAULT_COLOR, parser.style().background);
}

void checkColorSequences() {
	SgrParser parser;
	CHECK_TRUE(feedAll(parser, GREEN_SEQUENCE) == Result::STYLE_CHANGED);
	CHECK_EQ(static_cast<uint8_t>(Color::GREEN), parser.style().foreground);

	CHECK_TRUE(feedAll(parser, BOLD_RED_SEQUENCE) == Result::STYLE_CHANGED);
	CHECK_EQ(static_cast<uint8_t>(Color::RED), parser.style().foreground);
	CHECK_TRUE(parser.style().bold);

	feedAll(parser, BRIGHT_CYAN_SEQUENCE);
	CHECK_EQ(BRIGHT_CYAN, parser.style().foreground);

	feedAll(parser, BLUE_BACKGROUND_SEQUENCE);
	CHECK_EQ(static_cast<uint8_t>(Color::BLUE), parser.style().background);

	feedAll(parser, RESET_SEQUENCE);
	CHECK_EQ(DEFAULT_COLOR, parser.style().foreground);
	CHECK_EQ(DEFAULT_COLOR, parser.style().background);
	CHECK_FALSE(parser.style().bold);

	feedAll(parser, GREEN_SEQUENCE);
	feedAll(parser, EMPTY_RESET_SEQUENCE);
	CHECK_EQ(DEFAULT_COLOR, parser.style().foreground);
}

void checkSequenceShowsNothing() {
	SgrParser parser;
	CHECK_EQ(NOTHING, countPrinted(parser, GREEN_SEQUENCE));
	CHECK_EQ(NOTHING, countPrinted(parser, BOLD_RED_SEQUENCE));
	CHECK_EQ(NOTHING, countPrinted(parser, CURSOR_SEQUENCE));
}

void checkOtherSequencesAreDropped() {
	SgrParser parser;
	CHECK_TRUE(feedAll(parser, CURSOR_SEQUENCE) == Result::CONSUMED);
	CHECK_EQ(DEFAULT_COLOR, parser.style().foreground);
}

void checkSplitSequence() {
	SgrParser parser;
	CHECK_TRUE(parser.feed(GREEN_SEQUENCE[FIRST_BYTE]) == Result::CONSUMED);
	CHECK_TRUE(parser.feed(GREEN_SEQUENCE[FIRST_BYTE + 1]) == Result::CONSUMED);
	CHECK_EQ(DEFAULT_COLOR, parser.style().foreground);
	CHECK_TRUE(parser.feed(GREEN_SEQUENCE[FIRST_BYTE + 2]) == Result::CONSUMED);
	CHECK_TRUE(parser.feed(GREEN_SEQUENCE[FIRST_BYTE + 3]) == Result::CONSUMED);
	CHECK_TRUE(parser.feed(GREEN_SEQUENCE[FIRST_BYTE + 4]) == Result::STYLE_CHANGED);
	CHECK_EQ(static_cast<uint8_t>(Color::GREEN), parser.style().foreground);
}

void checkBrokenSequenceIsCancelled() {
	SgrParser parser;
	feedAll(parser, BROKEN_SEQUENCE);
	CHECK_TRUE(parser.feed('a') == Result::PRINT);

	CHECK_TRUE(feedAll(parser, NOT_A_SEQUENCE) == Result::PRINT);
	CHECK_TRUE(parser.feed('b') == Result::PRINT);
	CHECK_EQ(DEFAULT_COLOR, parser.style().foreground);
}

void checkHugeNumberIsBounded() {
	SgrParser parser;
	CHECK_TRUE(feedAll(parser, LONG_SEQUENCE) == Result::STYLE_CHANGED);
	CHECK_EQ(DEFAULT_COLOR, parser.style().foreground);
	CHECK_TRUE(parser.feed('a') == Result::PRINT);
}

} // namespace

void testAnsi() {
	checkPlainTextPasses();
	checkColorSequences();
	checkSequenceShowsNothing();
	checkOtherSequencesAreDropped();
	checkSplitSequence();
	checkBrokenSequenceIsCancelled();
	checkHugeNumberIsBounded();
}

} // namespace host_test
