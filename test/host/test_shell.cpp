#include <cock/shell/words.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <string.h>

namespace host_test {

using namespace cock::shell;

namespace {

constexpr size_t BUFFER_BYTES = 32;
constexpr size_t SMALL_BUFFER_BYTES = 8;
constexpr size_t NO_WORDS = 0;
constexpr size_t ONE_WORD = 1;
constexpr size_t TWO_WORDS = 2;
constexpr size_t THREE_WORDS = 3;
constexpr size_t FIRST = 0;
constexpr size_t SECOND = 1;
constexpr size_t THIRD = 2;
constexpr size_t FOURTH = 3;
constexpr size_t FIFTH = 4;
constexpr size_t FIVE_WORDS = 5;
constexpr size_t FROM_SECOND = 1;
constexpr uint32_t BIGGEST = 4294967295u;
constexpr uint32_t SAMPLE_NUMBER = 120;

bool sameText(const char *left, const char *right) {
	return strcmp(left, right) == 0;
}

void checkSplit() {
	char line[] = "echo  hello\tworld";
	Words words = {};
	CHECK_TRUE(split(line, words) == SplitResult::OK);
	CHECK_EQ(THREE_WORDS, words.count);
	CHECK_TRUE(sameText(words.items[FIRST], "echo"));
	CHECK_TRUE(sameText(words.items[SECOND], "hello"));
	CHECK_TRUE(sameText(words.items[THIRD], "world"));
}

void checkSplitEdges() {
	char empty[] = "";
	char blanks[] = "   \t ";
	char single[] = "  ls ";
	Words words = {};

	CHECK_TRUE(split(empty, words) == SplitResult::OK);
	CHECK_EQ(NO_WORDS, words.count);
	CHECK_TRUE(split(blanks, words) == SplitResult::OK);
	CHECK_EQ(NO_WORDS, words.count);
	CHECK_TRUE(split(single, words) == SplitResult::OK);
	CHECK_EQ(ONE_WORD, words.count);
	CHECK_TRUE(sameText(words.items[FIRST], "ls"));
}

void checkTooManyWords() {
	char line[] = "a b c d e f g h i j k l m n o p q";
	Words words = {};
	CHECK_TRUE(split(line, words) == SplitResult::TOO_MANY_WORDS);

	char exact[] = "a b c d e f g h i j k l m n o p";
	CHECK_TRUE(split(exact, words) == SplitResult::OK);
	CHECK_EQ(MAX_WORDS, words.count);
}

void checkQuotes() {
	char line[] = "echo 'a  b' \"c d\" e\"f g\"h ''";
	Words words = {};
	CHECK_TRUE(split(line, words) == SplitResult::OK);
	CHECK_EQ(FIVE_WORDS, words.count);
	CHECK_TRUE(sameText(words.items[SECOND], "a  b"));
	CHECK_TRUE(sameText(words.items[THIRD], "c d"));
	CHECK_TRUE(sameText(words.items[FOURTH], "ef gh"));
	CHECK_TRUE(sameText(words.items[FIFTH], ""));
}

void checkEscapes() {
	char line[] = "a\\ b \"x\\\"y\\\\z\\q\" 'k\\n' tail\\";
	Words words = {};
	CHECK_TRUE(split(line, words) == SplitResult::OK);
	CHECK_EQ(FIVE_WORDS - ONE_WORD, words.count);
	CHECK_TRUE(sameText(words.items[FIRST], "a b"));
	CHECK_TRUE(sameText(words.items[SECOND], "x\"y\\z\\q"));
	CHECK_TRUE(sameText(words.items[THIRD], "k\\n"));
	CHECK_TRUE(sameText(words.items[FOURTH], "tail\\"));
}

void checkUnterminatedQuote() {
	char single[] = "echo 'abc";
	char twice[] = "echo \"abc'";
	Words words = {};
	CHECK_TRUE(split(single, words) == SplitResult::UNTERMINATED_QUOTE);
	CHECK_TRUE(split(twice, words) == SplitResult::UNTERMINATED_QUOTE);
}

void checkParseNumber() {
	uint32_t value = 0;
	CHECK_TRUE(parseNumber("120", value));
	CHECK_EQ(SAMPLE_NUMBER, value);
	CHECK_TRUE(parseNumber("4294967295", value));
	CHECK_EQ(BIGGEST, value);
	CHECK_FALSE(parseNumber("4294967296", value));
	CHECK_FALSE(parseNumber("", value));
	CHECK_FALSE(parseNumber("12a", value));
	CHECK_FALSE(parseNumber("-1", value));
}

void checkJoin() {
	char line[] = "write file one two";
	Words words = {};
	char buffer[BUFFER_BYTES] = {};
	size_t length = 0;

	CHECK_TRUE(split(line, words) == SplitResult::OK);
	CHECK_TRUE(join(words, TWO_WORDS, buffer, sizeof(buffer), length));
	CHECK_TRUE(sameText(buffer, "one two"));
	CHECK_EQ(strlen(buffer), length);

	CHECK_TRUE(join(words, FROM_SECOND, buffer, sizeof(buffer), length));
	CHECK_TRUE(sameText(buffer, "file one two"));

	CHECK_TRUE(join(words, words.count, buffer, sizeof(buffer), length));
	CHECK_EQ(NO_WORDS, length);
}

void checkJoinLimit() {
	char line[] = "aaaa bbbb cccc";
	Words words = {};
	char buffer[SMALL_BUFFER_BYTES] = {};
	size_t length = 0;

	CHECK_TRUE(split(line, words) == SplitResult::OK);
	CHECK_FALSE(join(words, FIRST, buffer, sizeof(buffer), length));
}

} // namespace

void testShell() {
	checkSplit();
	checkSplitEdges();
	checkTooManyWords();
	checkQuotes();
	checkEscapes();
	checkUnterminatedQuote();
	checkParseNumber();
	checkJoin();
	checkJoinLimit();
}

} // namespace host_test
