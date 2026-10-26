#include <cat/copy.hpp>
#include <cock/user/tool.hpp>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

using namespace cock::user::tool;

namespace cock::cat {

namespace {

constexpr size_t CHUNK_BYTES = 512;
constexpr ssize_t END_OF_FILE = 0;
constexpr char STANDARD_INPUT_NAME[] = "standard input";

bool writeAll(const char *chunk, size_t count) {
	size_t done = 0;
	while (done < count) {
		ssize_t written = ::write(STDOUT_DESCRIPTOR, chunk + done, count - done);
		if (written <= END_OF_FILE) return false;
		done += static_cast<size_t>(written);
	}
	return true;
}

int copyDescriptor(const char *program, const char *subject, int descriptor) {
	char chunk[CHUNK_BYTES] = {};
	for (;;) {
		ssize_t count = ::read(descriptor, chunk, sizeof(chunk));
		if (count == END_OF_FILE) return STATUS_OK;
		if (count < END_OF_FILE || !writeAll(chunk, static_cast<size_t>(count))) return fail(program, subject);
	}
}

} // namespace

int copyFile(const char *program, const char *path) {
	int descriptor = ::open(path, O_RDONLY);
	if (descriptor < 0) return fail(program, path);

	int status = copyDescriptor(program, path, descriptor);
	::close(descriptor);
	return status;
}

int copyStandardInput(const char *program) {
	return copyDescriptor(program, STANDARD_INPUT_NAME, STDIN_DESCRIPTOR);
}

} // namespace cock::cat
