#include <cock/user/tool.hpp>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <write/command.hpp>

using namespace cock::user::tool;

namespace cock::write {

namespace {

constexpr char SEPARATOR = ' ';
constexpr char NEWLINE = '\n';
constexpr int TEXT_INDEX = 2;
constexpr size_t TEXT_BYTES = 512;
constexpr size_t NO_BYTES = 0;
constexpr ssize_t NO_PROGRESS = 0;

bool join(int argc, char **argv, char (&text)[TEXT_BYTES], size_t &length) {
	length = NO_BYTES;
	for (int index = TEXT_INDEX; index < argc; index++) {
		size_t word = strlen(argv[index]);
		size_t needed = word + sizeof(SEPARATOR);
		if (length + needed >= sizeof(text)) return false;
		if (index > TEXT_INDEX) text[length++] = SEPARATOR;
		memcpy(text + length, argv[index], word);
		length += word;
	}
	text[length++] = NEWLINE;
	return true;
}

bool putText(int descriptor, const char *text, size_t length) {
	size_t done = NO_BYTES;
	while (done < length) {
		ssize_t written = ::write(descriptor, text + done, length - done);
		if (written <= NO_PROGRESS) return false;
		done += static_cast<size_t>(written);
	}
	return true;
}

} // namespace

int run(int argc, char **argv) {
	if (argumentCount(argc) < TWO_ARGUMENTS) return usage(argv[0], "path text...");

	char text[TEXT_BYTES] = {};
	size_t length = NO_BYTES;
	if (!join(argc, argv, text, length)) {
		printf("%s: text too long\n", argv[0]);
		return STATUS_FAILURE;
	}

	const char *path = argv[FIRST_ARGUMENT];
	int descriptor = ::open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (descriptor < 0) return fail(argv[0], path);

	int status = putText(descriptor, text, length) ? STATUS_OK : fail(argv[0], path);
	::close(descriptor);
	return status;
}

} // namespace cock::write
