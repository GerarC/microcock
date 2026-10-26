#include <cock/shell/expand.hpp>
#include <cock/shell/variables.hpp>
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

constexpr char NAME_HOME[] = "HOME";
constexpr char VALUE_HOME[] = "/my dir";
constexpr char NAME_STATUS[] = "?";
constexpr char VALUE_STATUS[] = "127";
constexpr size_t TINY_BUFFER_BYTES = 6;
constexpr size_t NAME_HOME_LENGTH = sizeof(NAME_HOME) - sizeof(char);
constexpr size_t BIG_BUFFER_BYTES = 64;
constexpr size_t ASSIGNMENT_NAME_LENGTH = 3;
constexpr char FILLER = 'n';

const char *testLookup(const char *name, size_t length) {
	if (length == NAME_HOME_LENGTH && memcmp(name, NAME_HOME, length) == 0) return VALUE_HOME;
	if (length == sizeof(NAME_STATUS) - sizeof(char) && memcmp(name, NAME_STATUS, length) == 0) return VALUE_STATUS;
	return nullptr;
}

bool expandsTo(const char *line, const char *expected) {
	char out[BIG_BUFFER_BYTES] = {};
	return expand(line, out, sizeof(out), testLookup) == ExpandResult::OK && sameText(out, expected);
}

void checkExpand() {
	CHECK_TRUE(expandsTo("echo $?", "echo 127"));
	CHECK_TRUE(expandsTo("echo $HOME/x", "echo /my\\ dir/x"));
	CHECK_TRUE(expandsTo("echo \"$HOME\"", "echo \"/my dir\""));
	CHECK_TRUE(expandsTo("echo '$HOME'", "echo '$HOME'"));
	CHECK_TRUE(expandsTo("echo \\$HOME", "echo \\$HOME"));
	CHECK_TRUE(expandsTo("echo \"\\$HOME\"", "echo \"$HOME\""));
	CHECK_TRUE(expandsTo("echo $UNSET|", "echo |"));
	CHECK_TRUE(expandsTo("echo $ $1 a$", "echo $ $1 a$"));
}

void checkExpandThenSplit() {
	char out[BIG_BUFFER_BYTES] = {};
	Words words = {};

	CHECK_TRUE(expand("echo $HOME a", out, sizeof(out), testLookup) == ExpandResult::OK);
	CHECK_TRUE(split(out, words) == SplitResult::OK);
	CHECK_EQ(THREE_WORDS, words.count);
	CHECK_TRUE(sameText(words.items[SECOND], VALUE_HOME));
}

void checkExpandLimit() {
	char out[TINY_BUFFER_BYTES] = {};

	CHECK_TRUE(expand("ab", out, sizeof(out), testLookup) == ExpandResult::OK);
	CHECK_TRUE(expand("$HOME", out, sizeof(out), testLookup) == ExpandResult::TOO_LONG);
	CHECK_TRUE(expand("abcdef", out, sizeof(out), testLookup) == ExpandResult::TOO_LONG);
}

void checkVariables() {
	Variables table;
	size_t length = 0;

	CHECK_TRUE(isAssignment("A_1=x", length));
	CHECK_EQ(ASSIGNMENT_NAME_LENGTH, length);
	CHECK_FALSE(isAssignment("1A=x", length));
	CHECK_FALSE(isAssignment("=x", length));
	CHECK_FALSE(isAssignment("abc", length));

	CHECK_TRUE(table.set("ab", TWO_WORDS, "one") == SetResult::OK);
	CHECK_TRUE(sameText(table.get("ab", TWO_WORDS), "one"));
	CHECK_TRUE(table.set("ab", TWO_WORDS, "two") == SetResult::OK);
	CHECK_TRUE(sameText(table.get("ab", TWO_WORDS), "two"));
	CHECK_TRUE(table.get("a", ONE_WORD) == nullptr);
	CHECK_TRUE(table.unset("ab", TWO_WORDS));
	CHECK_FALSE(table.unset("ab", TWO_WORDS));
	CHECK_TRUE(table.get("ab", TWO_WORDS) == nullptr);
}

void checkVariablesLimits() {
	Variables table;
	char name[NAME_BYTES + 1] = {};
	char value[VALUE_BYTES + 1] = {};
	memset(name, FILLER, NAME_BYTES);
	memset(value, FILLER, VALUE_BYTES);

	CHECK_TRUE(table.set(name, NAME_BYTES, "x") == SetResult::TOO_LONG);
	CHECK_TRUE(table.set("a", ONE_WORD, value) == SetResult::TOO_LONG);

	for (size_t index = 0; index < MAX_VARIABLES; index++) {
		name[FIRST] = static_cast<char>(FIRST_LOWER + index);
		CHECK_TRUE(table.set(name, ONE_WORD, "x") == SetResult::OK);
	}
	CHECK_TRUE(table.set("zz", TWO_WORDS, "x") == SetResult::TABLE_FULL);
	CHECK_TRUE(table.set("a", ONE_WORD, "y") == SetResult::OK);
}

} // namespace

void testShell() {
	checkExpand();
	checkExpandThenSplit();
	checkExpandLimit();
	checkVariables();
	checkVariablesLimits();
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
