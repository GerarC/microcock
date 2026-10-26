#include <cock/shell/expand.hpp>
#include <cock/shell/variables.hpp>
#include <sh/arguments.hpp>
#include <sh/commands.hpp>
#include <sh/paths.hpp>
#include <sh/program_commands.hpp>
#include <sh/variables.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

using cock::shell::Words;

namespace cock::sh {

namespace {

constexpr uint32_t DECIMAL_BASE = 10;
constexpr size_t NUMBER_BYTES = 12;

cock::shell::Variables variables;
char statusText[NUMBER_BYTES] = {};
char directoryText[PATH_BYTES] = {};

bool sameName(const char *name, size_t length, const char *known) {
	size_t index = 0;
	while (index < length && known[index] != cock::shell::TERMINATOR && name[index] == known[index])
		index++;
	return index == length && known[index] == cock::shell::TERMINATOR;
}

const char *statusValue() {
	uint32_t status = static_cast<uint32_t>(lastStatus());
	size_t position = NUMBER_BYTES - sizeof(char);
	statusText[position] = cock::shell::TERMINATOR;
	do {
		statusText[--position] = static_cast<char>(cock::shell::FIRST_DIGIT + status % DECIMAL_BASE);
		status /= DECIMAL_BASE;
	} while (status != 0);
	return &statusText[position];
}

const char *directoryValue() {
	if (getcwd(directoryText, sizeof(directoryText)) == nullptr) return nullptr;
	return directoryText;
}

const char *lookup(const char *name, size_t length) {
	if (sameName(name, length, cock::shell::STATUS_NAME)) return statusValue();
	if (sameName(name, length, cock::shell::DIRECTORY_NAME)) return directoryValue();
	return variables.get(name, length);
}

} // namespace

bool expandLine(const char *line, char (&out)[cock::shell::MAX_LINE_BYTES]) {
	if (cock::shell::expand(line, out, sizeof(out), lookup) == cock::shell::ExpandResult::OK) return true;
	printf("sh: line too long\n");
	return false;
}

bool assignVariables(const Words &words) {
	size_t lengths[cock::shell::MAX_WORDS] = {};
	for (size_t index = 0; index < words.count; index++)
		if (!cock::shell::isAssignment(words.items[index], lengths[index])) return false;

	for (size_t index = 0; index < words.count; index++) {
		const char *word = words.items[index];
		cock::shell::SetResult result = variables.set(word, lengths[index], word + lengths[index] + sizeof(char));
		if (result == cock::shell::SetResult::TOO_LONG) printf("sh: assignment too long\n");
		if (result == cock::shell::SetResult::TABLE_FULL) printf("sh: too many variables\n");
	}
	return true;
}

void unsetCommand(const Words &words) {
	if (words.count < ONE_ARGUMENT) return usage(words.items[FIRST_WORD], "name...");

	for (size_t index = SECOND_WORD; index < words.count; index++) {
		const char *name = words.items[index];
		size_t length = 0;
		while (name[length] != cock::shell::TERMINATOR)
			length++;
		variables.unset(name, length);
	}
}

void setCommand(const Words &) {
	for (size_t index = 0; index < cock::shell::MAX_VARIABLES; index++) {
		const char *name = nullptr;
		const char *value = nullptr;
		if (variables.at(index, name, value)) printf("%s=%s\n", name, value);
	}
}

} // namespace cock::sh
