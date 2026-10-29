#include <cock/user/tool.hpp>
#include <grep/matcher.hpp>
#include <string.h>

using namespace cock::user::tool;

namespace cock::grep {

namespace {

constexpr char NEWLINE = '\n';
constexpr char LABEL_END[] = ":";
constexpr char LINE_END[] = "\n";
constexpr char OUTPUT_NAME[] = "standard output";

bool contains(const char *line, size_t length, const char *text, size_t textLength) {
	if (textLength > length) return false;
	for (size_t start = 0; start + textLength <= length; start++)
		if (memcmp(line + start, text, textLength) == 0) return true;
	return false;
}

bool writeText(const char *text, size_t length) {
	return writeAll(STDOUT_DESCRIPTOR, text, length);
}

} // namespace

LineMatcher::LineMatcher(const char *program, const char *text, const char *label)
	: program(program), text(text), textLength(strlen(text)), label(label) {}

Flow LineMatcher::endLine() {
	bool hit = contains(line, length, text, textLength);
	size_t printed = length;
	length = 0;
	if (!hit) return Flow::MORE;

	found = true;
	bool written = true;
	if (label != nullptr) written = writeText(label, strlen(label)) && writeText(LABEL_END, strlen(LABEL_END));
	written = written && writeText(line, printed) && writeText(LINE_END, strlen(LINE_END));
	if (written) return Flow::MORE;

	fail(program, OUTPUT_NAME);
	return Flow::ERROR;
}

Flow LineMatcher::add(const char *chunk, size_t count) {
	for (size_t index = 0; index < count; index++) {
		if (chunk[index] == NEWLINE) {
			if (endLine() == Flow::ERROR) return Flow::ERROR;
		} else if (length < LINE_BYTES) {
			line[length++] = chunk[index];
		}
	}
	return Flow::MORE;
}

Flow LineMatcher::finish() {
	return length == 0 ? Flow::MORE : endLine();
}

} // namespace cock::grep
